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

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include <gmock/gmock.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/dynamic_partition_control_interface.h"
#include "update_engine/dynamic_partition_control_android.h"

namespace chromeos_update_engine {

class MockDynamicPartitionControl : public DynamicPartitionControlInterface {
 public:
  MOCK_METHOD5(MapPartitionOnDeviceMapper,
               bool(const std::string&,
                    const std::string&,
                    uint32_t,
                    bool,
                    std::string*));
  MOCK_METHOD0(Cleanup, void());
  MOCK_METHOD0(GetDynamicPartitionsFeatureFlag, FeatureFlag());
  MOCK_METHOD5(
      PreparePartitionsForUpdate,
      bool(uint32_t, uint32_t, const DeltaArchiveManifest&, bool, uint64_t*));
  MOCK_METHOD0(GetVirtualAbFeatureFlag, FeatureFlag());
  MOCK_METHOD1(FinishUpdate, bool(bool));
  MOCK_METHOD0(CleanupSuccessfulUpdate, ErrorCode());
  MOCK_METHOD3(GetCleanupPreviousUpdateAction,
               std::unique_ptr<AbstractAction>(
                   BootControlInterface*,
                   PrefsInterface*,
                   CleanupPreviousUpdateActionDelegateInterface*));
};

class MockDynamicPartitionControlAndroid
    : public DynamicPartitionControlAndroid {
 public:
  MOCK_METHOD5(MapPartitionOnDeviceMapper,
               bool(const std::string&,
                    const std::string&,
                    uint32_t,
                    bool,
                    std::string*));
  MOCK_METHOD1(UnmapPartitionOnDeviceMapper, bool(const std::string&));
  MOCK_METHOD0(Cleanup, void());
  MOCK_METHOD1(DeviceExists, bool(const std::string&));
  MOCK_METHOD1(GetState, ::android::dm::DmDeviceState(const std::string&));
  MOCK_METHOD2(GetDmDevicePathByName, bool(const std::string&, std::string*));
  MOCK_METHOD3(LoadMetadataBuilder,
               std::unique_ptr<::android::fs_mgr::MetadataBuilder>(
                   const std::string&, uint32_t, uint32_t));
  MOCK_METHOD3(StoreMetadata,
               bool(const std::string&,
                    android::fs_mgr::MetadataBuilder*,
                    uint32_t));
  MOCK_METHOD1(GetDeviceDir, bool(std::string*));
  MOCK_METHOD0(GetDynamicPartitionsFeatureFlag, FeatureFlag());
  MOCK_METHOD1(GetSuperPartitionName, std::string(uint32_t));
  MOCK_METHOD0(GetVirtualAbFeatureFlag, FeatureFlag());
  MOCK_METHOD1(FinishUpdate, bool(bool));
  MOCK_METHOD5(
      GetSystemOtherPath,
      bool(uint32_t, uint32_t, const std::string&, std::string*, bool*));
  MOCK_METHOD2(EraseSystemOtherAvbFooter, bool(uint32_t, uint32_t));
  MOCK_METHOD0(IsAvbEnabledOnSystemOther, std::optional<bool>());

  void set_fake_mapped_devices(const std::set<std::string>& fake) override {
    DynamicPartitionControlAndroid::set_fake_mapped_devices(fake);
  }

  bool RealGetSystemOtherPath(uint32_t source_slot,
                              uint32_t target_slot,
                              const std::string& partition_name_suffix,
                              std::string* path,
                              bool* should_unmap) {
    return DynamicPartitionControlAndroid::GetSystemOtherPath(
        source_slot, target_slot, partition_name_suffix, path, should_unmap);
  }

  bool RealEraseSystemOtherAvbFooter(uint32_t source_slot,
                                     uint32_t target_slot) {
    return DynamicPartitionControlAndroid::EraseSystemOtherAvbFooter(
        source_slot, target_slot);
  }

  std::optional<bool> RealIsAvbEnabledInFstab(const std::string& path) {
    return DynamicPartitionControlAndroid::IsAvbEnabledInFstab(path);
  }
};

}  // namespace chromeos_update_engine
