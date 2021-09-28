//
// Copyright (C) 2020 The Android Open Source Project
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
#include "update_engine/cleanup_previous_update_action.h"

#include <chrono>  // NOLINT(build/c++11) -- for merge times
#include <functional>
#include <string>
#include <type_traits>

#include <android-base/properties.h>
#include <base/bind.h>

#ifndef __ANDROID_RECOVERY__
#include <statslog.h>
#endif

#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/delta_performer.h"

using android::base::GetBoolProperty;
using android::snapshot::SnapshotManager;
using android::snapshot::SnapshotMergeStats;
using android::snapshot::UpdateState;
using brillo::MessageLoop;

constexpr char kBootCompletedProp[] = "sys.boot_completed";
// Interval to check sys.boot_completed.
constexpr auto kCheckBootCompletedInterval = base::TimeDelta::FromSeconds(2);
// Interval to check IBootControl::isSlotMarkedSuccessful
constexpr auto kCheckSlotMarkedSuccessfulInterval =
    base::TimeDelta::FromSeconds(2);
// Interval to call SnapshotManager::ProcessUpdateState
constexpr auto kWaitForMergeInterval = base::TimeDelta::FromSeconds(2);

#ifdef __ANDROID_RECOVERY__
static constexpr bool kIsRecovery = true;
#else
static constexpr bool kIsRecovery = false;
#endif

namespace chromeos_update_engine {

CleanupPreviousUpdateAction::CleanupPreviousUpdateAction(
    PrefsInterface* prefs,
    BootControlInterface* boot_control,
    android::snapshot::SnapshotManager* snapshot,
    CleanupPreviousUpdateActionDelegateInterface* delegate)
    : prefs_(prefs),
      boot_control_(boot_control),
      snapshot_(snapshot),
      delegate_(delegate),
      running_(false),
      cancel_failed_(false),
      last_percentage_(0),
      merge_stats_(SnapshotMergeStats::GetInstance(*snapshot)) {}

void CleanupPreviousUpdateAction::PerformAction() {
  ResumeAction();
}

void CleanupPreviousUpdateAction::TerminateProcessing() {
  SuspendAction();
}

void CleanupPreviousUpdateAction::ResumeAction() {
  CHECK(prefs_);
  CHECK(boot_control_);

  LOG(INFO) << "Starting/resuming CleanupPreviousUpdateAction";
  running_ = true;
  StartActionInternal();
}

void CleanupPreviousUpdateAction::SuspendAction() {
  LOG(INFO) << "Stopping/suspending CleanupPreviousUpdateAction";
  running_ = false;
}

void CleanupPreviousUpdateAction::ActionCompleted(ErrorCode error_code) {
  running_ = false;
  ReportMergeStats();
  metadata_device_ = nullptr;
}

std::string CleanupPreviousUpdateAction::Type() const {
  return StaticType();
}

std::string CleanupPreviousUpdateAction::StaticType() {
  return "CleanupPreviousUpdateAction";
}

void CleanupPreviousUpdateAction::StartActionInternal() {
  // Do nothing on non-VAB device.
  if (!boot_control_->GetDynamicPartitionControl()
           ->GetVirtualAbFeatureFlag()
           .IsEnabled()) {
    processor_->ActionComplete(this, ErrorCode::kSuccess);
    return;
  }
  // SnapshotManager is only available on VAB devices.
  CHECK(snapshot_);
  WaitBootCompletedOrSchedule();
}

void CleanupPreviousUpdateAction::ScheduleWaitBootCompleted() {
  TEST_AND_RETURN(running_);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CleanupPreviousUpdateAction::WaitBootCompletedOrSchedule,
                 base::Unretained(this)),
      kCheckBootCompletedInterval);
}

void CleanupPreviousUpdateAction::WaitBootCompletedOrSchedule() {
  TEST_AND_RETURN(running_);
  if (!kIsRecovery &&
      !android::base::GetBoolProperty(kBootCompletedProp, false)) {
    // repeat
    ScheduleWaitBootCompleted();
    return;
  }

  LOG(INFO) << "Boot completed, waiting on markBootSuccessful()";
  CheckSlotMarkedSuccessfulOrSchedule();
}

void CleanupPreviousUpdateAction::ScheduleWaitMarkBootSuccessful() {
  TEST_AND_RETURN(running_);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &CleanupPreviousUpdateAction::CheckSlotMarkedSuccessfulOrSchedule,
          base::Unretained(this)),
      kCheckSlotMarkedSuccessfulInterval);
}

void CleanupPreviousUpdateAction::CheckSlotMarkedSuccessfulOrSchedule() {
  TEST_AND_RETURN(running_);
  if (!kIsRecovery &&
      !boot_control_->IsSlotMarkedSuccessful(boot_control_->GetCurrentSlot())) {
    ScheduleWaitMarkBootSuccessful();
    return;
  }

  if (metadata_device_ == nullptr) {
    metadata_device_ = snapshot_->EnsureMetadataMounted();
  }

  if (metadata_device_ == nullptr) {
    LOG(ERROR) << "Failed to mount /metadata.";
    // If metadata is erased but not formatted, it is possible to not mount
    // it in recovery. It is safe to skip CleanupPreviousUpdateAction.
    processor_->ActionComplete(
        this, kIsRecovery ? ErrorCode::kSuccess : ErrorCode::kError);
    return;
  }

  if (kIsRecovery) {
    auto snapshots_created =
        snapshot_->RecoveryCreateSnapshotDevices(metadata_device_);
    switch (snapshots_created) {
      case android::snapshot::CreateResult::CREATED: {
        // If previous update has not finished merging, snapshots exists and are
        // created here so that ProcessUpdateState can proceed.
        LOG(INFO) << "Snapshot devices are created";
        break;
      }
      case android::snapshot::CreateResult::NOT_CREATED: {
        // If there is no previous update, no snapshot devices are created and
        // ProcessUpdateState will return immediately. Hence, NOT_CREATED is not
        // considered an error.
        LOG(INFO) << "Snapshot devices are not created";
        break;
      }
      case android::snapshot::CreateResult::ERROR:
      default: {
        LOG(ERROR)
            << "Failed to create snapshot devices (CreateResult = "
            << static_cast<
                   std::underlying_type_t<android::snapshot::CreateResult>>(
                   snapshots_created);
        processor_->ActionComplete(this, ErrorCode::kError);
        return;
      }
    }
  }

  if (!merge_stats_->Start()) {
    // Not an error because CleanupPreviousUpdateAction may be paused and
    // resumed while kernel continues merging snapshots in the background.
    LOG(WARNING) << "SnapshotMergeStats::Start failed.";
  }
  LOG(INFO) << "Waiting for any previous merge request to complete. "
            << "This can take up to several minutes.";
  WaitForMergeOrSchedule();
}

void CleanupPreviousUpdateAction::ScheduleWaitForMerge() {
  TEST_AND_RETURN(running_);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CleanupPreviousUpdateAction::WaitForMergeOrSchedule,
                 base::Unretained(this)),
      kWaitForMergeInterval);
}

void CleanupPreviousUpdateAction::WaitForMergeOrSchedule() {
  TEST_AND_RETURN(running_);
  auto state = snapshot_->ProcessUpdateState(
      std::bind(&CleanupPreviousUpdateAction::OnMergePercentageUpdate, this),
      std::bind(&CleanupPreviousUpdateAction::BeforeCancel, this));
  merge_stats_->set_state(state);

  switch (state) {
    case UpdateState::None: {
      LOG(INFO) << "Can't find any snapshot to merge.";
      ErrorCode error_code = ErrorCode::kSuccess;
      if (!snapshot_->CancelUpdate()) {
        error_code = ErrorCode::kError;
        LOG(INFO) << "Failed to call SnapshotManager::CancelUpdate().";
      }
      processor_->ActionComplete(this, error_code);
      return;
    }

    case UpdateState::Initiated: {
      LOG(ERROR) << "Previous update has not been completed, not cleaning up";
      processor_->ActionComplete(this, ErrorCode::kSuccess);
      return;
    }

    case UpdateState::Unverified: {
      InitiateMergeAndWait();
      return;
    }

    case UpdateState::Merging: {
      ScheduleWaitForMerge();
      return;
    }

    case UpdateState::MergeNeedsReboot: {
      LOG(ERROR) << "Need reboot to finish merging.";
      processor_->ActionComplete(this, ErrorCode::kError);
      return;
    }

    case UpdateState::MergeCompleted: {
      LOG(INFO) << "Merge finished with state MergeCompleted.";
      processor_->ActionComplete(this, ErrorCode::kSuccess);
      return;
    }

    case UpdateState::MergeFailed: {
      LOG(ERROR) << "Merge failed. Device may be corrupted.";
      processor_->ActionComplete(this, ErrorCode::kDeviceCorrupted);
      return;
    }

    case UpdateState::Cancelled: {
      // DeltaPerformer::ResetUpdateProgress failed, hence snapshots are
      // not deleted to avoid inconsistency.
      // Nothing can be done here; just try next time.
      ErrorCode error_code =
          cancel_failed_ ? ErrorCode::kError : ErrorCode::kSuccess;
      processor_->ActionComplete(this, error_code);
      return;
    }

    default: {
      // Protobuf has some reserved enum values, so a default case is needed.
      LOG(FATAL) << "SnapshotManager::ProcessUpdateState returns "
                 << static_cast<int32_t>(state);
    }
  }
}

bool CleanupPreviousUpdateAction::OnMergePercentageUpdate() {
  double percentage = 0.0;
  snapshot_->GetUpdateState(&percentage);
  if (delegate_) {
    // libsnapshot uses [0, 100] percentage but update_engine uses [0, 1].
    delegate_->OnCleanupProgressUpdate(percentage / 100);
  }

  // Log if percentage increments by at least 1.
  if (last_percentage_ < static_cast<unsigned int>(percentage)) {
    last_percentage_ = percentage;
    LOG(INFO) << "Waiting for merge to complete: " << last_percentage_ << "%.";
  }

  // Do not continue to wait for merge. Instead, let ProcessUpdateState
  // return Merging directly so that we can ScheduleWaitForMerge() in
  // MessageLoop.
  return false;
}

bool CleanupPreviousUpdateAction::BeforeCancel() {
  if (DeltaPerformer::ResetUpdateProgress(
          prefs_,
          false /* quick */,
          false /* skip dynamic partitions metadata*/)) {
    return true;
  }

  // ResetUpdateProgress might not work on stub prefs. Do additional checks.
  LOG(WARNING) << "ProcessUpdateState returns Cancelled but cleanup failed.";

  std::string val;
  ignore_result(prefs_->GetString(kPrefsDynamicPartitionMetadataUpdated, &val));
  if (val.empty()) {
    LOG(INFO) << kPrefsDynamicPartitionMetadataUpdated
              << " is empty, assuming successful cleanup";
    return true;
  }
  LOG(WARNING)
      << kPrefsDynamicPartitionMetadataUpdated << " is " << val
      << ", not deleting snapshots even though UpdateState is Cancelled.";
  cancel_failed_ = true;
  return false;
}

void CleanupPreviousUpdateAction::InitiateMergeAndWait() {
  TEST_AND_RETURN(running_);
  LOG(INFO) << "Attempting to initiate merge.";
  // suspend the VAB merge when running a DSU
  if (GetBoolProperty("ro.gsid.image_running", false)) {
    LOG(WARNING) << "Suspend the VAB merge when running a DSU.";
    processor_->ActionComplete(this, ErrorCode::kError);
    return;
  }

  uint64_t cow_file_size;
  if (snapshot_->InitiateMerge(&cow_file_size)) {
    merge_stats_->set_cow_file_size(cow_file_size);
    WaitForMergeOrSchedule();
    return;
  }

  LOG(WARNING) << "InitiateMerge failed.";
  auto state = snapshot_->GetUpdateState();
  merge_stats_->set_state(state);
  if (state == UpdateState::Unverified) {
    // We are stuck at unverified state. This can happen if the update has
    // been applied, but it has not even been attempted yet (in libsnapshot,
    // rollback indicator does not exist); for example, if update_engine
    // restarts before the device reboots, then this state may be reached.
    // Nothing should be done here.
    LOG(WARNING) << "InitiateMerge leaves the device at "
                 << "UpdateState::Unverified. (Did update_engine "
                 << "restarted?)";
    processor_->ActionComplete(this, ErrorCode::kSuccess);
    return;
  }

  // State does seems to be advanced.
  // It is possibly racy. For example, on a userdebug build, the user may
  // manually initiate a merge with snapshotctl between last time
  // update_engine checks UpdateState. Hence, just call
  // WaitForMergeOrSchedule one more time.
  LOG(WARNING) << "IniitateMerge failed but GetUpdateState returned "
               << android::snapshot::UpdateState_Name(state)
               << ", try to wait for merge again.";
  WaitForMergeOrSchedule();
  return;
}

void CleanupPreviousUpdateAction::ReportMergeStats() {
  auto result = merge_stats_->Finish();
  if (result == nullptr) {
    LOG(WARNING) << "Not reporting merge stats because "
                    "SnapshotMergeStats::Finish failed.";
    return;
  }

#ifdef __ANDROID_RECOVERY__
  LOG(INFO) << "Skip reporting merge stats in recovery.";
#else
  const auto& report = result->report();

  if (report.state() == UpdateState::None ||
      report.state() == UpdateState::Initiated ||
      report.state() == UpdateState::Unverified) {
    LOG(INFO) << "Not reporting merge stats because state is "
              << android::snapshot::UpdateState_Name(report.state());
    return;
  }

  auto passed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      result->merge_time());

  bool vab_retrofit = boot_control_->GetDynamicPartitionControl()
                          ->GetVirtualAbFeatureFlag()
                          .IsRetrofit();

  LOG(INFO) << "Reporting merge stats: "
            << android::snapshot::UpdateState_Name(report.state()) << " in "
            << passed_ms.count() << "ms (resumed " << report.resume_count()
            << " times), using " << report.cow_file_size()
            << " bytes of COW image.";
  android::util::stats_write(android::util::SNAPSHOT_MERGE_REPORTED,
                             static_cast<int32_t>(report.state()),
                             static_cast<int64_t>(passed_ms.count()),
                             static_cast<int32_t>(report.resume_count()),
                             vab_retrofit,
                             static_cast<int64_t>(report.cow_file_size()));
#endif
}

}  // namespace chromeos_update_engine
