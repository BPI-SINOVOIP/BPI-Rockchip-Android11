#include <errno.h>
#include <string.h>

#include <cutils/properties.h>
#include <hardware/boot_control.h>
#include <hardware/hardware.h>

#include <libavb_ab/libavb_ab.h>
#include <libavb_user/libavb_user.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>

#include <bootloader_message/bootloader_message.h>
#include <android/hardware/boot/1.1/IBootControl.h>


#include "rk_boot_control.h"

#define LOG_TAG "Rockchip bootctrl"

#include <log/log.h>

namespace android {
namespace bootable {

using ::android::hardware::boot::V1_1::MergeStatus;

static AvbOps* ops = NULL;

// Initialize the boot_control_private struct with the information from
// the bootloader_message buffer stored in |boot_ctrl|. Returns whether the
// initialization succeeded.
bool BootControl::Init() {
  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  LOG(INFO) << "rk BootControl Init ";

  if (!InitMiscVirtualAbMessageIfNeeded()) {
    LOG(ERROR) << "rk BootControl Init InitMiscVirtualAbMessageIfNeeded failed ";
    return false;
  }

  if (ops != NULL) {
    return true;
  }

  ops = avb_ops_user_new();
  if (ops == NULL) {
    LOG(ERROR) << "rk BootControl Init Unable to allocate AvbOps instance. ";
  }
  return true;
}

unsigned int BootControl::GetNumberSlots() {
  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  LOG(INFO) << "rk BootControl GetNumberSlots ";
  return 2;
}

unsigned int BootControl::GetCurrentSlot() {
  //char propbuf[PROPERTY_VALUE_MAX];

  LOG(INFO) << "rk BootControl GetCurrentSlot ";
  //property_get("ro.boot.slot_suffix", propbuf, "");
  std::string suffix_prop = android::base::GetProperty("ro.boot.slot_suffix", "");
  if (suffix_prop.empty()) {
    LOG(ERROR) << "rk BootControl GetCurrentSlot Slot suffix property is not set";
    return 0;
  }
  

  if (strcmp(suffix_prop.c_str(), "_a") == 0) {
    return 0;
  } else if (strcmp(suffix_prop.c_str(), "_b") == 0) {
    return 1;
  } else {
    avb_errorv("rk BootControl GetCurrentSlot Unexpected slot suffix '", suffix_prop.c_str(), "'.\n", NULL);
    LOG(ERROR) << "rk BootControl GetCurrentSlot Unexpected slot suffix: '" << suffix_prop.c_str() << "' ";
    return 0;
  }
  return 0;
}

bool BootControl::MarkBootSuccessful() {
  setenv("ANDROID_LOG_TAGS", "*:v", 1);
  LOG(INFO) << "rk BootControl MarkBootSuccessful ";
  if (avb_ab_mark_slot_successful(ops->ab_ops, GetCurrentSlot()) ==
      AVB_IO_RESULT_OK) {
    return 0;
  } else {
    return -EIO;
  }
}

bool BootControl::SetActiveBootSlot(unsigned int slot) {
  LOG(INFO) << "rk BootControl SetActiveBootSlot ";
  if (slot >= GetNumberSlots()) {
    return -EINVAL;
  } else if (avb_ab_mark_slot_active(ops->ab_ops, slot) == AVB_IO_RESULT_OK) {
    return 0;
  } else {
    return -EIO;
  }
}

bool BootControl::SetSlotAsUnbootable(unsigned int slot) {
  LOG(INFO) << "rk BootControl SetSlotAsUnbootable ";
  if (slot >= GetNumberSlots()) {
    return -EINVAL;
  } else if (avb_ab_mark_slot_unbootable(ops->ab_ops, slot) ==
             AVB_IO_RESULT_OK) {
    return 0;
  } else {
    return -EIO;
  }
}

bool BootControl::IsSlotBootable(unsigned int slot) {
  AvbABData ab_data;
  bool is_bootable;

  LOG(INFO) << "rk BootControl IsSlotBootable ";

  if (slot >= GetNumberSlots()) {
    return -EINVAL;
  } else if (avb_ab_data_read(ops->ab_ops, &ab_data) != AVB_IO_RESULT_OK) {
    return -EIO;
  }

  is_bootable = (ab_data.slots[slot].priority > 0) &&
                (ab_data.slots[slot].successful_boot ||
                 (ab_data.slots[slot].tries_remaining > 0));

  return is_bootable ? 1 : 0;
}

bool BootControl::IsSlotMarkedSuccessful(unsigned int slot) {
  AvbABData ab_data;
  bool is_marked_successful;

  LOG(INFO) << "rk BootControl IsSlotMarkedSuccessful ";

  if (slot >= GetNumberSlots()) {
    return -EINVAL;
  } else if (avb_ab_data_read(ops->ab_ops, &ab_data) != AVB_IO_RESULT_OK) {
    return -EIO;
  }

  is_marked_successful = ab_data.slots[slot].successful_boot;

  return is_marked_successful ? 1 : 0;
}

bool BootControl::IsValidSlot(unsigned int slot) {
  LOG(INFO) << "rk BootControl IsValidSlot ";

  return slot < 2;
}

const char* BootControl::GetSuffix(unsigned int slot) {
  static const char* suffix[2] = {"_a", "_b"};

  LOG(INFO) << "rk BootControl GetSuffix ";

  if (slot >= 2) {
    return NULL;
  }
  return suffix[slot];
}

bool BootControl::SetSnapshotMergeStatus(MergeStatus status) {
  LOG(INFO) << "rk BootControl SetSnapshotMergeStatus ";
  return SetMiscVirtualAbMergeStatus(GetCurrentSlot(), status);
}

MergeStatus BootControl::GetSnapshotMergeStatus() {
  MergeStatus status;

  LOG(INFO) << "rk BootControl GetSnapshotMergeStatus ";
  if (!GetMiscVirtualAbMergeStatus(GetCurrentSlot(), &status)) {
    return MergeStatus::UNKNOWN;
  }
  return status;
}



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
bool InitMiscVirtualAbMessageIfNeeded() {
  std::string err;
  misc_virtual_ab_message message;
  if (!ReadMiscVirtualAbMessage(&message, &err)) {
    LOG(ERROR) << "Could not read merge status: " << err;
    return false;
  }

  if (message.version == MISC_VIRTUAL_AB_MESSAGE_VERSION &&
      message.magic == MISC_VIRTUAL_AB_MAGIC_HEADER) {
    // Already initialized.
    return true;
  }

  message = {};
  message.version = MISC_VIRTUAL_AB_MESSAGE_VERSION;
  message.magic = MISC_VIRTUAL_AB_MAGIC_HEADER;
  if (!WriteMiscVirtualAbMessage(message, &err)) {
    LOG(ERROR) << "Could not write merge status: " << err;
    return false;
  }
  return true;
}


// Save the current merge status as well as the current slot.
bool SetMiscVirtualAbMergeStatus(unsigned int current_slot,
                                 android::hardware::boot::V1_1::MergeStatus status) {
  std::string err;
  misc_virtual_ab_message message;

  if (!ReadMiscVirtualAbMessage(&message, &err)) {
    LOG(ERROR) << "Could not read merge status: " << err;
    return false;
  }

  message.merge_status = static_cast<uint8_t>(status);
  message.source_slot = current_slot;
  if (!WriteMiscVirtualAbMessage(message, &err)) {
    LOG(ERROR) << "Could not write merge status: " << err;
    return false;
  }
  return true;
}


// Return the current merge status. If the saved status is SNAPSHOTTED but the
// slot hasn't changed, the status returned will be NONE.
bool GetMiscVirtualAbMergeStatus(unsigned int current_slot,
                                 android::hardware::boot::V1_1::MergeStatus* status) {
  std::string err;
  misc_virtual_ab_message message;

  if (!ReadMiscVirtualAbMessage(&message, &err)) {
    LOG(ERROR) << "Could not read merge status: " << err;
    return false;
  }

  // If the slot reverted after having created a snapshot, then the snapshot will
  // be thrown away at boot. Thus we don't count this as being in a snapshotted
  // state.
  *status = static_cast<MergeStatus>(message.merge_status);
  if (*status == MergeStatus::SNAPSHOTTED && current_slot == message.source_slot) {
    *status = MergeStatus::NONE;
  }
  return true;
}

}  // namespace bootable
}  // namespace android
