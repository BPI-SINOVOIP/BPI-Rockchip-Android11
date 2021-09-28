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

#ifndef UPDATE_ENGINE_CLEANUP_PREVIOUS_UPDATE_ACTION_H_
#define UPDATE_ENGINE_CLEANUP_PREVIOUS_UPDATE_ACTION_H_

#include <chrono>  // NOLINT(build/c++11) -- for merge times
#include <memory>
#include <string>

#include <brillo/message_loops/message_loop.h>
#include <libsnapshot/snapshot.h>
#include <libsnapshot/snapshot_stats.h>

#include "update_engine/common/action.h"
#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/cleanup_previous_update_action_delegate.h"
#include "update_engine/common/error_code.h"
#include "update_engine/common/prefs_interface.h"

namespace chromeos_update_engine {

class CleanupPreviousUpdateAction;

template <>
class ActionTraits<CleanupPreviousUpdateAction> {
 public:
  typedef NoneType InputObjectType;
  typedef NoneType OutputObjectType;
};

// On Android Virtual A/B devices, clean up snapshots from previous update
// attempt. See DynamicPartitionControlAndroid::CleanupSuccessfulUpdate.
class CleanupPreviousUpdateAction : public Action<CleanupPreviousUpdateAction> {
 public:
  CleanupPreviousUpdateAction(
      PrefsInterface* prefs,
      BootControlInterface* boot_control,
      android::snapshot::SnapshotManager* snapshot,
      CleanupPreviousUpdateActionDelegateInterface* delegate);

  void PerformAction() override;
  void SuspendAction() override;
  void ResumeAction() override;
  void TerminateProcessing() override;
  void ActionCompleted(ErrorCode error_code) override;
  std::string Type() const override;
  static std::string StaticType();
  typedef ActionTraits<CleanupPreviousUpdateAction>::InputObjectType
      InputObjectType;
  typedef ActionTraits<CleanupPreviousUpdateAction>::OutputObjectType
      OutputObjectType;

 private:
  PrefsInterface* prefs_;
  BootControlInterface* boot_control_;
  android::snapshot::SnapshotManager* snapshot_;
  CleanupPreviousUpdateActionDelegateInterface* delegate_;
  std::unique_ptr<android::snapshot::AutoDevice> metadata_device_;
  bool running_{false};
  bool cancel_failed_{false};
  unsigned int last_percentage_{0};
  android::snapshot::SnapshotMergeStats* merge_stats_;

  void StartActionInternal();
  void ScheduleWaitBootCompleted();
  void WaitBootCompletedOrSchedule();
  void ScheduleWaitMarkBootSuccessful();
  void CheckSlotMarkedSuccessfulOrSchedule();
  void ScheduleWaitForMerge();
  void WaitForMergeOrSchedule();
  void InitiateMergeAndWait();
  void ReportMergeStats();

  // Callbacks to ProcessUpdateState.
  bool OnMergePercentageUpdate();
  bool BeforeCancel();
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CLEANUP_PREVIOUS_UPDATE_ACTION_H_
