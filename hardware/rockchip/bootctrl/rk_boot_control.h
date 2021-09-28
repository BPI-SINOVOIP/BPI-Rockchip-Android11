#pragma once

#include <string>

#include <android/hardware/boot/1.1/IBootControl.h>

namespace android {
namespace bootable {

// Helper library to implement the IBootControl HAL using the misc partition.
class BootControl {
  using MergeStatus = ::android::hardware::boot::V1_1::MergeStatus;

 public:
  bool Init();
  unsigned int GetNumberSlots();
  unsigned int GetCurrentSlot();
  bool MarkBootSuccessful();
  bool SetActiveBootSlot(unsigned int slot);
  bool SetSlotAsUnbootable(unsigned int slot);
  bool SetSlotBootable(unsigned int slot);
  bool IsSlotBootable(unsigned int slot);
  const char* GetSuffix(unsigned int slot);
  bool IsSlotMarkedSuccessful(unsigned int slot);
  bool SetSnapshotMergeStatus(MergeStatus status);
  MergeStatus GetSnapshotMergeStatus();

  bool IsValidSlot(unsigned int slot);
};

// Helper functions to write the Virtual A/B merge status message. These are
// separate because BootControl uses bootloader_control_ab in vendor space,
// whereas the Virtual A/B merge status is in system space. A HAL might not
// use bootloader_control_ab, but may want to use the AOSP method of maintaining
// the merge status.

// If the Virtual A/B message has not yet been initialized, then initialize it.
// This should be called when the BootControl HAL first loads.
//
// If the Virtual A/B message in misc was already initialized, true is returned.
// If initialization was attempted, but failed, false is returned, and the HAL
// should fail to load.
bool InitMiscVirtualAbMessageIfNeeded();

// Save the current merge status as well as the current slot.
bool SetMiscVirtualAbMergeStatus(unsigned int current_slot,
                                 android::hardware::boot::V1_1::MergeStatus status);

// Return the current merge status. If the saved status is SNAPSHOTTED but the
// slot hasn't changed, the status returned will be NONE.
bool GetMiscVirtualAbMergeStatus(unsigned int current_slot,
                                 android::hardware::boot::V1_1::MergeStatus* status);

}  // namespace bootable
}  // namespace android
