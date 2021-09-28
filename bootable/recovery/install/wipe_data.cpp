/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "install/wipe_data.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <functional>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "install/snapshot_utils.h"
#include "otautil/dirutil.h"
#include "recovery_ui/ui.h"
#include "recovery_utils/logging.h"
#include "recovery_utils/roots.h"
#include "mtdutils/rk29.h"
constexpr const char* CACHE_ROOT = "/cache";
constexpr const char* DATA_ROOT = "/data";
constexpr const char* METADATA_ROOT = "/metadata";

/**
 * reset hdmi after restore factory.
*/
#define BASEPARAMER_PARTITION_NAME "/baseparameter"
#define BASEPARAMER_PARTITION_SIZE 1024*1024/2

int erase_baseparameter() {
    Volume* v = volume_for_mount_point(BASEPARAMER_PARTITION_NAME);
    if (v == NULL) {
        printf("unknown volume baseparamer, not erase baseparamer\n");
        return -1;
    }

    int file;
    file = open((v->blk_device).c_str(), O_RDWR);
    if (file < 0){
        printf("baseparamer file can not be opened");
        return -1;
    }
    lseek(file, BASEPARAMER_PARTITION_SIZE, SEEK_SET);

    //size of baseparameter.
    char buf[BASEPARAMER_PARTITION_SIZE];
    memset(buf, 0, BASEPARAMER_PARTITION_SIZE);
    read(file, buf, BASEPARAMER_PARTITION_SIZE);

    lseek(file, 0L, SEEK_SET);
    write(file, (char*)(&buf), BASEPARAMER_PARTITION_SIZE);
    close(file);
    sync();

    return 0;
}

static bool EraseVolume(const char* volume, RecoveryUI* ui, bool convert_fbe) {
  bool is_cache = (strcmp(volume, CACHE_ROOT) == 0);
  bool is_data = (strcmp(volume, DATA_ROOT) == 0);

  ui->SetBackground(RecoveryUI::ERASING);
  ui->SetProgressType(RecoveryUI::INDETERMINATE);

  std::vector<saved_log_file> log_files;
  if (is_cache) {
    // If we're reformatting /cache, we load any past logs (i.e. "/cache/recovery/last_*") and the
    // current log ("/cache/recovery/log") into memory, so we can restore them after the reformat.
    log_files = ReadLogFilesToMemory();
  }

  ui->Print("Formatting %s...\n", volume);

  ensure_path_unmounted(volume);

  int result;
  if (is_data && convert_fbe) {
    constexpr const char* CONVERT_FBE_DIR = "/tmp/convert_fbe";
    constexpr const char* CONVERT_FBE_FILE = "/tmp/convert_fbe/convert_fbe";
    // Create convert_fbe breadcrumb file to signal init to convert to file based encryption, not
    // full disk encryption.
    if (mkdir(CONVERT_FBE_DIR, 0700) != 0) {
      PLOG(ERROR) << "Failed to mkdir " << CONVERT_FBE_DIR;
      return false;
    }
    FILE* f = fopen(CONVERT_FBE_FILE, "wbe");
    if (!f) {
      PLOG(ERROR) << "Failed to convert to file encryption";
      return false;
    }
    fclose(f);
    result = format_volume(volume, CONVERT_FBE_DIR);
    remove(CONVERT_FBE_FILE);
    rmdir(CONVERT_FBE_DIR);
  } else {
    result = format_volume(volume);
  }

  if (is_cache) {
    RestoreLogFilesAfterFormat(log_files);
  }

  return (result == 0);
}

bool WipeCache(RecoveryUI* ui, const std::function<bool()>& confirm_func) {
  bool has_cache = volume_for_mount_point("/cache") != nullptr;
  if (!has_cache) {
    ui->Print("No /cache partition found.\n");
    return false;
  }

  if (confirm_func && !confirm_func()) {
    return false;
  }

  ui->Print("\n-- Wiping cache...\n");
  bool success = EraseVolume("/cache", ui, false);
  ui->Print("Cache wipe %s.\n", success ? "complete" : "failed");
  return success;
}

bool WipeData(Device* device, bool convert_fbe) {
  RecoveryUI* ui = device->GetUI();
  ui->Print("\n-- Wiping data...\n");

  if (!FinishPendingSnapshotMerges(device)) {
    ui->Print("Unable to check update status or complete merge, cannot wipe partitions.\n");
    return false;
  }

  bool success = device->PreWipeData();
  if (success) {
    success &= EraseVolume(DATA_ROOT, ui, convert_fbe);
    bool has_cache = volume_for_mount_point("/cache") != nullptr;
    if (has_cache) {
      success &= EraseVolume(CACHE_ROOT, ui, false);
    }
    if (volume_for_mount_point(METADATA_ROOT) != nullptr) {
      success &= EraseVolume(METADATA_ROOT, ui, false);
    }
  }
  if (success) {
    success &= device->PostWipeData();
  }
  erase_baseparameter();
  ui->Print("Data wipe %s.\n", success ? "complete" : "failed");
  return success;
}

void SureMetadataMount() {
  if (ensure_path_mounted(METADATA_ROOT)) {
    printf("mount metadata fail,so formate...\n");
    reset_tmplog_offset();
    format_volume(METADATA_ROOT);
    ensure_path_mounted(METADATA_ROOT);
  }
}
void WipeFrp() {
  printf("begin to wipe frp partion!\n");
  int ret = format_volume("/frp");
  if(ret<0){
    printf("wiping frp failed!\n");
  } else {
    printf("wiping frp success!\n");
  }
}

int ResizeData(Device* device){
  Volume* v11 = volume_for_mount_point("/data");
  int result = 0;
  RecoveryUI* ui = device->GetUI();
  ui->Print("\n-- Resize data...\n");

  if (nullptr == v11){
    printf("ResizeData failed! v11 is NULL \n");
	ui->Print("\n-- Resize failed v11 is NULL...\n");
    return -1;
  }

  ui->SetBackground(RecoveryUI::ERASING);
  ui->SetProgressType(RecoveryUI::INDETERMINATE);

  ui->Print("Resizing %s...\n", (v11->blk_device).c_str());
  printf("ResizeData blk_device=%s \n", (v11->blk_device).c_str());
  if(strcmp((v11->fs_type).c_str(), (char*)"f2fs") == 0){
    if(rk_check_and_resizefs_f2fs((v11->blk_device).c_str())) {
      printf("check and resize /data failed! blk_device=%s \n", (v11->blk_device).c_str());
      result = -1;
    }
  }else{
    if(rk_check_and_resizefs((v11->blk_device).c_str())) {
      printf("check and resize /data failed! blk_device=%s \n", (v11->blk_device).c_str());
      result = -1;
    }
  }

  ui->Print("\n-- Resize Complete...\n");
  return result;
}
