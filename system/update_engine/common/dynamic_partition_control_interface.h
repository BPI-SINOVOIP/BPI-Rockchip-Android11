//
// Copyright (C) 2018 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_COMMON_DYNAMIC_PARTITION_CONTROL_INTERFACE_H_
#define UPDATE_ENGINE_COMMON_DYNAMIC_PARTITION_CONTROL_INTERFACE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "update_engine/common/action.h"
#include "update_engine/common/cleanup_previous_update_action_delegate.h"
#include "update_engine/common/error_code.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

struct FeatureFlag {
  enum class Value { NONE = 0, RETROFIT, LAUNCH };
  constexpr explicit FeatureFlag(Value value) : value_(value) {}
  constexpr bool IsEnabled() const { return value_ != Value::NONE; }
  constexpr bool IsRetrofit() const { return value_ == Value::RETROFIT; }
  constexpr bool IsLaunch() const { return value_ == Value::LAUNCH; }

 private:
  Value value_;
};

class BootControlInterface;
class PrefsInterface;

class DynamicPartitionControlInterface {
 public:
  virtual ~DynamicPartitionControlInterface() = default;

  // Return the feature flags of dynamic partitions on this device.
  // Return RETROFIT iff dynamic partitions is retrofitted on this device,
  //        LAUNCH iff this device is launched with dynamic partitions,
  //        NONE iff dynamic partitions is disabled on this device.
  virtual FeatureFlag GetDynamicPartitionsFeatureFlag() = 0;

  // Return the feature flags of Virtual A/B on this device.
  virtual FeatureFlag GetVirtualAbFeatureFlag() = 0;

  // Attempt to optimize |operation|.
  // If successful, |optimized| contains an operation with extents that
  // needs to be written.
  // If failed, no optimization is available, and caller should perform
  // |operation| directly.
  // |partition_name| should not have the slot suffix; implementation of
  // DynamicPartitionControlInterface checks partition at the target slot
  // previously set with PreparePartitionsForUpdate().
  virtual bool OptimizeOperation(const std::string& partition_name,
                                 const InstallOperation& operation,
                                 InstallOperation* optimized) = 0;

  // Do necessary cleanups before destroying the object.
  virtual void Cleanup() = 0;

  // Prepare all partitions for an update specified in |manifest|.
  // This is needed before calling MapPartitionOnDeviceMapper(), otherwise the
  // device would be mapped in an inconsistent way.
  // If |update| is set, create snapshots and writes super partition metadata.
  // If |required_size| is not null and call fails due to insufficient space,
  // |required_size| will be set to total free space required on userdata
  // partition to apply the update. Otherwise (call succeeds, or fails
  // due to other errors), |required_size| is set to zero.
  virtual bool PreparePartitionsForUpdate(uint32_t source_slot,
                                          uint32_t target_slot,
                                          const DeltaArchiveManifest& manifest,
                                          bool update,
                                          uint64_t* required_size) = 0;

  // After writing to new partitions, before rebooting into the new slot, call
  // this function to indicate writes to new partitions are done.
  virtual bool FinishUpdate(bool powerwash_required) = 0;

  // Get an action to clean up previous update.
  // Return NoOpAction on non-Virtual A/B devices.
  // Before applying the next update, run this action to clean up previous
  // update files. This function blocks until delta files are merged into
  // current OS partitions and finished cleaning up.
  // - If successful, action completes with kSuccess.
  // - If any error, but caller should retry after reboot, action completes with
  //   kError.
  // - If any irrecoverable failures, action completes with kDeviceCorrupted.
  //
  // See ResetUpdate for differences between CleanuPreviousUpdateAction and
  // ResetUpdate.
  virtual std::unique_ptr<AbstractAction> GetCleanupPreviousUpdateAction(
      BootControlInterface* boot_control,
      PrefsInterface* prefs,
      CleanupPreviousUpdateActionDelegateInterface* delegate) = 0;

  // Called after an unwanted payload has been successfully applied and the
  // device has not yet been rebooted.
  //
  // For snapshot updates (Virtual A/B), it calls
  // DeltaPerformer::ResetUpdateProgress(false /* quick */) and
  // frees previously allocated space; the next update will need to be
  // started over.
  //
  // Note: CleanupPreviousUpdateAction does not do anything if an update is in
  // progress, while ResetUpdate() forcefully free previously
  // allocated space for snapshot updates.
  virtual bool ResetUpdate(PrefsInterface* prefs) = 0;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_DYNAMIC_PARTITION_CONTROL_INTERFACE_H_
