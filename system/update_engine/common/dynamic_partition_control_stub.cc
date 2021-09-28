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

#include <stdint.h>

#include <memory>
#include <string>

#include <base/logging.h>

#include "update_engine/common/dynamic_partition_control_stub.h"

namespace chromeos_update_engine {

FeatureFlag DynamicPartitionControlStub::GetDynamicPartitionsFeatureFlag() {
  return FeatureFlag(FeatureFlag::Value::NONE);
}

FeatureFlag DynamicPartitionControlStub::GetVirtualAbFeatureFlag() {
  return FeatureFlag(FeatureFlag::Value::NONE);
}

bool DynamicPartitionControlStub::OptimizeOperation(
    const std::string& partition_name,
    const InstallOperation& operation,
    InstallOperation* optimized) {
  return false;
}

void DynamicPartitionControlStub::Cleanup() {}

bool DynamicPartitionControlStub::PreparePartitionsForUpdate(
    uint32_t source_slot,
    uint32_t target_slot,
    const DeltaArchiveManifest& manifest,
    bool update,
    uint64_t* required_size) {
  return true;
}

bool DynamicPartitionControlStub::FinishUpdate(bool powerwash_required) {
  return true;
}

std::unique_ptr<AbstractAction>
DynamicPartitionControlStub::GetCleanupPreviousUpdateAction(
    BootControlInterface* boot_control,
    PrefsInterface* prefs,
    CleanupPreviousUpdateActionDelegateInterface* delegate) {
  return std::make_unique<NoOpAction>();
}

bool DynamicPartitionControlStub::ResetUpdate(PrefsInterface* prefs) {
  return false;
}

}  // namespace chromeos_update_engine
