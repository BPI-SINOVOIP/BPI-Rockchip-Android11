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

#ifndef UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_
#define UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_

#include <memory>
#include <set>
#include <string>

#include <base/files/file_util.h>
#include <libsnapshot/auto_device.h>
#include <libsnapshot/snapshot.h>

#include "update_engine/common/dynamic_partition_control_interface.h"

namespace chromeos_update_engine {

class DynamicPartitionControlAndroid : public DynamicPartitionControlInterface {
 public:
  DynamicPartitionControlAndroid();
  ~DynamicPartitionControlAndroid();
  FeatureFlag GetDynamicPartitionsFeatureFlag() override;
  FeatureFlag GetVirtualAbFeatureFlag() override;
  bool OptimizeOperation(const std::string& partition_name,
                         const InstallOperation& operation,
                         InstallOperation* optimized) override;
  void Cleanup() override;

  bool PreparePartitionsForUpdate(uint32_t source_slot,
                                  uint32_t target_slot,
                                  const DeltaArchiveManifest& manifest,
                                  bool update,
                                  uint64_t* required_size) override;
  bool FinishUpdate(bool powerwash_required) override;
  std::unique_ptr<AbstractAction> GetCleanupPreviousUpdateAction(
      BootControlInterface* boot_control,
      PrefsInterface* prefs,
      CleanupPreviousUpdateActionDelegateInterface* delegate) override;

  bool ResetUpdate(PrefsInterface* prefs) override;

  // Return the device for partition |partition_name| at slot |slot|.
  // |current_slot| should be set to the current active slot.
  // Note: this function is only used by BootControl*::GetPartitionDevice.
  // Other callers should prefer BootControl*::GetPartitionDevice over
  // BootControl*::GetDynamicPartitionControl()->GetPartitionDevice().
  bool GetPartitionDevice(const std::string& partition_name,
                          uint32_t slot,
                          uint32_t current_slot,
                          std::string* device);

 protected:
  // These functions are exposed for testing.

  // Unmap logical partition on device mapper. This is the reverse operation
  // of MapPartitionOnDeviceMapper.
  // Returns true if unmapped successfully.
  virtual bool UnmapPartitionOnDeviceMapper(
      const std::string& target_partition_name);

  // Retrieve metadata from |super_device| at slot |source_slot|.
  //
  // If |target_slot| != kInvalidSlot, before returning the metadata, this
  // function modifies the metadata so that during updates, the metadata can be
  // written to |target_slot|. In particular, on retrofit devices, the returned
  // metadata automatically includes block devices at |target_slot|.
  //
  // If |target_slot| == kInvalidSlot, this function returns metadata at
  // |source_slot| without modifying it. This is the same as
  // LoadMetadataBuilder(const std::string&, uint32_t).
  virtual std::unique_ptr<android::fs_mgr::MetadataBuilder> LoadMetadataBuilder(
      const std::string& super_device,
      uint32_t source_slot,
      uint32_t target_slot);

  // Write metadata |builder| to |super_device| at slot |target_slot|.
  virtual bool StoreMetadata(const std::string& super_device,
                             android::fs_mgr::MetadataBuilder* builder,
                             uint32_t target_slot);

  // Map logical partition on device-mapper.
  // |super_device| is the device path of the physical partition ("super").
  // |target_partition_name| is the identifier used in metadata; for example,
  // "vendor_a"
  // |slot| is the selected slot to mount; for example, 0 for "_a".
  // Returns true if mapped successfully; if so, |path| is set to the device
  // path of the mapped logical partition.
  virtual bool MapPartitionOnDeviceMapper(
      const std::string& super_device,
      const std::string& target_partition_name,
      uint32_t slot,
      bool force_writable,
      std::string* path);

  // Return true if a static partition exists at device path |path|.
  virtual bool DeviceExists(const std::string& path);

  // Returns the current state of the underlying device mapper device
  // with given name.
  // One of INVALID, SUSPENDED or ACTIVE.
  virtual android::dm::DmDeviceState GetState(const std::string& name);

  // Returns the path to the device mapper device node in '/dev' corresponding
  // to 'name'. If the device does not exist, false is returned, and the path
  // parameter is not set.
  virtual bool GetDmDevicePathByName(const std::string& name,
                                     std::string* path);

  // Retrieve metadata from |super_device| at slot |source_slot|.
  virtual std::unique_ptr<android::fs_mgr::MetadataBuilder> LoadMetadataBuilder(
      const std::string& super_device, uint32_t source_slot);

  // Return a possible location for devices listed by name.
  virtual bool GetDeviceDir(std::string* path);

  // Return the name of the super partition (which stores super partition
  // metadata) for a given slot.
  virtual std::string GetSuperPartitionName(uint32_t slot);

  virtual void set_fake_mapped_devices(const std::set<std::string>& fake);

  // Allow mock objects to override this to test recovery mode.
  virtual bool IsRecovery();

  // Determine path for system_other partition.
  // |source_slot| should be current slot.
  // |target_slot| should be "other" slot.
  // |partition_name_suffix| should be "system" + suffix(|target_slot|).
  // Return true and set |path| if successful.
  // Set |path| to empty if no need to erase system_other.
  // Set |should_unmap| to true if path needs to be unmapped later.
  //
  // Note: system_other cannot use GetPartitionDevice or
  // GetDynamicPartitionDevice because:
  // - super partition metadata may be loaded from the source slot
  // - UPDATED flag needs to be check to skip erasing if partition is not
  //   created by flashing tools
  // - Snapshots from previous update attempts should not be used.
  virtual bool GetSystemOtherPath(uint32_t source_slot,
                                  uint32_t target_slot,
                                  const std::string& partition_name_suffix,
                                  std::string* path,
                                  bool* should_unmap);

  // Returns true if any entry in the fstab file in |path| has AVB enabled,
  // false if not enabled, and nullopt for any error.
  virtual std::optional<bool> IsAvbEnabledInFstab(const std::string& path);

  // Returns true if system_other has AVB enabled, false if not enabled, and
  // nullopt for any error.
  virtual std::optional<bool> IsAvbEnabledOnSystemOther();

  // Erase system_other partition that may contain system_other.img.
  // After the update, the content of system_other may be corrupted but with
  // valid AVB footer. If the update is rolled back and factory data reset is
  // triggered, system_b fails to be mapped with verity errors (see
  // b/152444348). Erase the system_other so that mapping system_other is
  // skipped.
  virtual bool EraseSystemOtherAvbFooter(uint32_t source_slot,
                                         uint32_t target_slot);

 private:
  friend class DynamicPartitionControlAndroidTest;

  void UnmapAllPartitions();
  bool MapPartitionInternal(const std::string& super_device,
                            const std::string& target_partition_name,
                            uint32_t slot,
                            bool force_writable,
                            std::string* path);

  // Update |builder| according to |partition_metadata|, assuming the device
  // does not have Virtual A/B.
  bool UpdatePartitionMetadata(android::fs_mgr::MetadataBuilder* builder,
                               uint32_t target_slot,
                               const DeltaArchiveManifest& manifest);

  // Helper for PreparePartitionsForUpdate. Used for devices with dynamic
  // partitions updating without snapshots.
  // If |delete_source| is set, source partitions are deleted before resizing
  // target partitions (using DeleteSourcePartitions).
  bool PrepareDynamicPartitionsForUpdate(uint32_t source_slot,
                                         uint32_t target_slot,
                                         const DeltaArchiveManifest& manifest,
                                         bool delete_source);

  // Helper for PreparePartitionsForUpdate. Used for snapshotted partitions for
  // Virtual A/B update.
  bool PrepareSnapshotPartitionsForUpdate(uint32_t source_slot,
                                          uint32_t target_slot,
                                          const DeltaArchiveManifest& manifest,
                                          uint64_t* required_size);

  enum class DynamicPartitionDeviceStatus {
    SUCCESS,
    ERROR,
    TRY_STATIC,
  };

  // Return SUCCESS and path in |device| if partition is dynamic.
  // Return ERROR if any error.
  // Return TRY_STATIC if caller should resolve the partition as a static
  // partition instead.
  DynamicPartitionDeviceStatus GetDynamicPartitionDevice(
      const base::FilePath& device_dir,
      const std::string& partition_name_suffix,
      uint32_t slot,
      uint32_t current_slot,
      std::string* device);

  // Return true if |partition_name_suffix| is a block device of
  // super partition metadata slot |slot|.
  bool IsSuperBlockDevice(const base::FilePath& device_dir,
                          uint32_t current_slot,
                          const std::string& partition_name_suffix);

  // If sideloading a full OTA, delete source partitions from |builder|.
  bool DeleteSourcePartitions(android::fs_mgr::MetadataBuilder* builder,
                              uint32_t source_slot,
                              const DeltaArchiveManifest& manifest);

  // Returns true if metadata is expected to be mounted, false otherwise.
  // Note that it returns false on non-Virtual A/B devices.
  //
  // Almost all functions of SnapshotManager depends on metadata being mounted.
  // - In Android mode for Virtual A/B devices, assume it is mounted. If not,
  //   let caller fails when calling into SnapshotManager.
  // - In recovery for Virtual A/B devices, it is possible that metadata is not
  //   formatted, hence it cannot be mounted. Caller should not call into
  //   SnapshotManager.
  // - On non-Virtual A/B devices, updates do not depend on metadata partition.
  //   Caller should not call into SnapshotManager.
  //
  // This function does NOT mount metadata partition. Use EnsureMetadataMounted
  // to mount metadata partition.
  bool ExpectMetadataMounted();

  // Ensure /metadata is mounted. Returns true if successful, false otherwise.
  //
  // Note that this function returns true on non-Virtual A/B devices without
  // doing anything.
  bool EnsureMetadataMounted();

  std::set<std::string> mapped_devices_;
  const FeatureFlag dynamic_partitions_;
  const FeatureFlag virtual_ab_;
  std::unique_ptr<android::snapshot::SnapshotManager> snapshot_;
  std::unique_ptr<android::snapshot::AutoDevice> metadata_device_;
  bool target_supports_snapshot_ = false;
  // Whether the target partitions should be loaded as dynamic partitions. Set
  // by PreparePartitionsForUpdate() per each update.
  bool is_target_dynamic_ = false;
  uint32_t source_slot_ = UINT32_MAX;
  uint32_t target_slot_ = UINT32_MAX;

  DISALLOW_COPY_AND_ASSIGN(DynamicPartitionControlAndroid);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DYNAMIC_PARTITION_CONTROL_ANDROID_H_
