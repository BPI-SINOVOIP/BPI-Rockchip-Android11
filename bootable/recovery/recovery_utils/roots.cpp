/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include "recovery_utils/roots.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <cryptfs.h>
#include <ext4_utils/wipe.h>
#include <fs_mgr.h>
#include <fs_mgr/roots.h>

#include "otautil/sysutil.h"
#include "rkutility/rktools.h"
#include "rkutility/sdboot.h"

using android::fs_mgr::Fstab;
using android::fs_mgr::FstabEntry;
using android::fs_mgr::ReadDefaultFstab;

static Fstab fstab;

constexpr const char* CACHE_ROOT = "/cache";

void load_volume_table() {
  if (!ReadDefaultFstab(&fstab)) {
    LOG(ERROR) << "Failed to read default fstab";
    return;
  }

  fstab.emplace_back(FstabEntry{
      .blk_device = "ramdisk",
      .mount_point = "/tmp",
      .fs_type = "ramdisk",
      .length = 0,
  });

  std::cout << "recovery filesystem table" << std::endl << "=========================" << std::endl;
  for (size_t i = 0; i < fstab.size(); ++i) {
    const auto& entry = fstab[i];
    std::cout << "  " << i << " " << entry.mount_point << " "
              << " " << entry.fs_type << " " << entry.blk_device << " " << entry.length
              << std::endl;
  }
  std::cout << std::endl;
}

Volume* volume_for_mount_point(const std::string& mount_point) {
  return android::fs_mgr::GetEntryForMountPoint(&fstab, mount_point);
}

// Mount the volume specified by path at the given mount_point.
int ensure_path_mounted_at(const std::string& path, const std::string& mount_point) {
  return android::fs_mgr::EnsurePathMounted(&fstab, path, mount_point) ? 0 : -1;
}

int ensure_path_mounted(const std::string& path) {
  Volume* v = nullptr;
  const char* mount_point = nullptr;
  printf("ensure_path_mounted path=%s \n", path.c_str());
  // Mount at the default mount point.
  if(strncmp(path.c_str(),(char*)EX_SDCARD_ROOT, strlen((char*)EX_SDCARD_ROOT)) != 0)
  {
    return android::fs_mgr::EnsurePathMounted(&fstab, path) ? 0 : -1;
  }
  else{
    printf("ensure_path_mounted sdcard, path=%s \n", path.c_str());
	v = volume_for_mount_point(path);
	if (!v) {
      printf("ensure_path_mounted No fstab entry for path=%s \n", path.c_str());
      return -1;
    }

    if (strcmp((v->fs_type).c_str(), (const char*)"ramdisk") == 0) {
      // The ramdisk is always mounted.
      return 0;
    }

    if (!rktools_scan_mounted_volumes()) {
      LOG(ERROR) << "Failed to scan mounted volumes";
      return -1;
    }

    if (!mount_point) {
      mount_point = (v->mount_point).c_str();
    }

    const MountedVolume* mv = rktools_find_mounted_volume_by_mount_point(mount_point);
    if (mv != nullptr) {
      // Volume is already mounted.
      return 0;
    }

    mkdir(mount_point, 0755);  // in case it doesn't already exist

    if(strcmp((v->fs_type).c_str(), (char*)"vfat") == 0){
      const char *blk_device = nullptr;
      blk_device = (v->blk_device).c_str();
      if(strcmp((char*)"/mnt/external_sd", (v->mount_point).c_str()) == 0){
        blk_device = getenv(SD_POINT_NAME);
        if(blk_device == NULL){
          setFlashPoint();
          blk_device = getenv(SD_POINT_NAME);
        }
      }
      if(NULL == blk_device){
        blk_device = (v->blk_device).c_str();
      }
      int result = mount((v->blk_device).c_str(), (v->mount_point).c_str(), (v->fs_type).c_str(),
                         MS_NOATIME | MS_NODEV | MS_NODIRATIME, (char*)"shortname=mixed,utf8");
      if (result == 0) return 0;
      printf("v->blk_device is %s\n",(v->blk_device).c_str());
      printf("v->blk_device is %s to vfat.\n",(v->blk_device).c_str());
      LOG(ERROR) << "trying mount fs"<< blk_device << " to vfat.";
      if (blk_device !=nullptr){
          result = mount(blk_device, (v->mount_point).c_str(), (v->fs_type).c_str(),
                         MS_NOATIME | MS_NODEV | MS_NODIRATIME, (char*)"shortname=mixed,utf8");
          if (result == 0) return 0;
      }
      printf("blk_device is %s\n",blk_device);
      LOG(ERROR) << "trying mount "<< v->blk_device << " to ntfs.";
      printf("v->blk_device is %s to ntfs.\n",(v->blk_device).c_str());
      result = mount((v->blk_device).c_str(), (v->mount_point).c_str(), (char*)"ntfs",
                     MS_NOATIME | MS_NODEV | MS_NODIRATIME, (char*)"");
      if (result == 0) return 0;
      if (blk_device !=nullptr){
          result = mount(blk_device, (v->mount_point).c_str(), (char*)"ntfs",
                         MS_NOATIME | MS_NODEV | MS_NODIRATIME, (char*)"");
          if (result == 0) return 0;
      }

      const char *sec_dev = (v->fs_options).c_str();
      if(strcmp((char*)"/mnt/external_sd", (v->mount_point).c_str()) == 0){
          sec_dev = getenv(SD_POINT_NAME_2);
      }
      if(sec_dev != NULL) {
        printf("v->blk_device is %s to fs_type=%s.\n",sec_dev, (v->fs_type).c_str());
        result = mount(sec_dev, (v->mount_point).c_str(), (v->fs_type).c_str(),
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, (char*)"shortname=mixed,utf8");
        if (result == 0) return 0;

        LOG(ERROR) << "tring mount " << sec_dev << " ntfs.";
        printf("v->blk_device is %s to ntfs.\n",sec_dev);
        result = mount(sec_dev, (v->mount_point).c_str(), (char*)"ntfs",
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if (result == 0) return 0;
      }
      LOG(ERROR) << "failed to mount " << v->mount_point << " error: " << strerror(errno);
      printf("failed to mount mount_point=%s, error:%s \n",(v->mount_point).c_str(), strerror(errno));
      return -1;
    }
  }

  printf("Reach end of ensure_path_mounted, mount failed!\n");
  return -1;
}

int ensure_path_unmounted(const std::string& path) {
  return android::fs_mgr::EnsurePathUnmounted(&fstab, path) ? 0 : -1;
}

static int exec_cmd(const std::vector<std::string>& args) {
  CHECK(!args.empty());
  auto argv = StringVectorToNullTerminatedArray(args);

  pid_t child;
  if ((child = fork()) == 0) {
    execv(argv[0], argv.data());
    _exit(EXIT_FAILURE);
  }

  int status;
  waitpid(child, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    LOG(ERROR) << args[0] << " failed with status " << WEXITSTATUS(status);
  }
  return WEXITSTATUS(status);
}

static int64_t get_file_size(int fd, uint64_t reserve_len) {
  struct stat buf;
  int ret = fstat(fd, &buf);
  if (ret) return 0;

  int64_t computed_size;
  if (S_ISREG(buf.st_mode)) {
    computed_size = buf.st_size - reserve_len;
  } else if (S_ISBLK(buf.st_mode)) {
    uint64_t block_device_size = get_block_device_size(fd);
    if (block_device_size < reserve_len ||
        block_device_size > std::numeric_limits<int64_t>::max()) {
      computed_size = 0;
    } else {
      computed_size = block_device_size - reserve_len;
    }
  } else {
    computed_size = 0;
  }

  return computed_size;
}

int format_volume(const std::string& volume, const std::string& directory) {
  const FstabEntry* v = android::fs_mgr::GetEntryForPath(&fstab, volume);
  if (v == nullptr) {
    LOG(ERROR) << "unknown volume \"" << volume << "\"";
    return -1;
  }
  if (v->fs_type == "ramdisk") {
    LOG(ERROR) << "can't format_volume \"" << volume << "\"";
    return -1;
  }
  if (v->mount_point != volume) {
    LOG(ERROR) << "can't give path \"" << volume << "\" to format_volume";
    return -1;
  }
  if (ensure_path_unmounted(volume) != 0) {
    LOG(ERROR) << "format_volume: Failed to unmount \"" << v->mount_point << "\"";
    return -1;
  }
  if (v->mount_point != "/frp" && v->fs_type != "ext4" && v->fs_type != "f2fs") {
    LOG(ERROR) << "format_volume: fs_type \"" << v->fs_type << "\" unsupported";
    return -1;
  }

  bool needs_casefold = false;
  bool needs_projid = false;

  if (volume == "/data") {
    needs_casefold = android::base::GetBoolProperty("external_storage.casefold.enabled", false);
    needs_projid = android::base::GetBoolProperty("external_storage.projid.enabled", false);
  }

  // If there's a key_loc that looks like a path, it should be a block device for storing encryption
  // metadata. Wipe it too.
  if (!v->key_loc.empty() && v->key_loc[0] == '/') {
    LOG(INFO) << "Wiping " << v->key_loc;
    int fd = open(v->key_loc.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
      PLOG(ERROR) << "format_volume: Failed to open " << v->key_loc;
      return -1;
    }
    wipe_block_device(fd, get_file_size(fd));
    close(fd);
  }

  int64_t length = 0;
  if (v->length > 0) {
    length = v->length;
  } else if (v->length < 0 || v->key_loc == "footer") {
    android::base::unique_fd fd(open(v->blk_device.c_str(), O_RDONLY));
    if (fd == -1) {
      PLOG(ERROR) << "format_volume: failed to open " << v->blk_device;
      return -1;
    }
    length = get_file_size(fd.get(), v->length ? -v->length : CRYPT_FOOTER_OFFSET);
    if (length <= 0) {
      LOG(ERROR) << "get_file_size: invalid size " << length << " for " << v->blk_device;
      return -1;
    }
  }

  if (v->fs_type == "ext4") {
    static constexpr int kBlockSize = 4096;
    std::vector<std::string> mke2fs_args = {
      "/system/bin/mke2fs", "-F", "-t", "ext4", "-b", std::to_string(kBlockSize),
    };

    // Project ID's require wider inodes. The Quotas themselves are enabled by tune2fs on boot.
    if (needs_projid) {
      mke2fs_args.push_back("-I");
      mke2fs_args.push_back("512");
    }

    if (v->fs_mgr_flags.ext_meta_csum) {
      mke2fs_args.push_back("-O");
      mke2fs_args.push_back("metadata_csum");
      mke2fs_args.push_back("-O");
      mke2fs_args.push_back("64bit");
      mke2fs_args.push_back("-O");
      mke2fs_args.push_back("extent");
    }

    int raid_stride = v->logical_blk_size / kBlockSize;
    int raid_stripe_width = v->erase_blk_size / kBlockSize;
    // stride should be the max of 8KB and logical block size
    if (v->logical_blk_size != 0 && v->logical_blk_size < 8192) {
      raid_stride = 8192 / kBlockSize;
    }
    if (v->erase_blk_size != 0 && v->logical_blk_size != 0) {
      mke2fs_args.push_back("-E");
      mke2fs_args.push_back(
          android::base::StringPrintf("stride=%d,stripe-width=%d", raid_stride, raid_stripe_width));
    }
    mke2fs_args.push_back(v->blk_device);
    if (length != 0) {
      mke2fs_args.push_back(std::to_string(length / kBlockSize));
    }

    int result = exec_cmd(mke2fs_args);
    if (result == 0 && !directory.empty()) {
      std::vector<std::string> e2fsdroid_args = {
        "/system/bin/e2fsdroid", "-e", "-f", directory, "-a", volume, v->blk_device,
      };
      result = exec_cmd(e2fsdroid_args);
    }

    if (result != 0) {
      PLOG(ERROR) << "format_volume: Failed to make ext4 on " << v->blk_device;
      return -1;
    }
    return 0;
  }

  if (v->mount_point == "/frp") {
    LOG(INFO) << "format_volume for: fs_type: " << v->fs_type <<  "blk_device: " << v->blk_device << "mount_point: " <<  v->mount_point;
    int fd = open((v->blk_device).c_str(), O_WRONLY);
    if (fd < 0){
        LOG(ERROR) << "format_volume: failed to open :  " <<  v->blk_device;
        return -1;
    }
    uint64_t len = get_block_device_size(fd);
    LOG(INFO) << "format_volume:len: " << len;
    if(wipe_block_device(fd,len) == 0 ) {
        LOG(INFO) << "format_volume: success to format : " << v->blk_device;
        return 0;
    }
    else{
        LOG(ERROR) << "format_volume: fail to format : " <<  v->blk_device;
        return -1;
    }
  }

  // Has to be f2fs because we checked earlier.
  static constexpr int kSectorSize = 4096;
  std::vector<std::string> make_f2fs_cmd = {
    "/system/bin/make_f2fs",
    "-g",
    "android",
  };
  if (needs_projid) {
    make_f2fs_cmd.push_back("-O");
    make_f2fs_cmd.push_back("project_quota,extra_attr");
  }
  if (needs_casefold) {
    make_f2fs_cmd.push_back("-O");
    make_f2fs_cmd.push_back("casefold");
    make_f2fs_cmd.push_back("-C");
    make_f2fs_cmd.push_back("utf8");
  }
  make_f2fs_cmd.push_back(v->blk_device);
  if (length >= kSectorSize) {
    make_f2fs_cmd.push_back(std::to_string(length / kSectorSize));
  }

  if (exec_cmd(make_f2fs_cmd) != 0) {
    PLOG(ERROR) << "format_volume: Failed to make_f2fs on " << v->blk_device;
    return -1;
  }
  if (!directory.empty()) {
    std::vector<std::string> sload_f2fs_cmd = {
      "/system/bin/sload_f2fs", "-f", directory, "-t", volume, v->blk_device,
    };
    if (exec_cmd(sload_f2fs_cmd) != 0) {
      PLOG(ERROR) << "format_volume: Failed to sload_f2fs on " << v->blk_device;
      return -1;
    }
  }
  return 0;
}

int format_volume(const std::string& volume) {
  return format_volume(volume, "");
}

int setup_install_mounts() {
  if (fstab.empty()) {
    LOG(ERROR) << "can't set up install mounts: no fstab loaded";
    return -1;
  }
  for (const FstabEntry& entry : fstab) {
    // We don't want to do anything with "/".
    if (entry.mount_point == "/") {
      continue;
    }

    if (entry.mount_point == "/tmp" || entry.mount_point == "/cache") {
      if (ensure_path_mounted(entry.mount_point) != 0) {
        LOG(ERROR) << "Failed to mount " << entry.mount_point;
        return -1;
      }
    } else {
      if((strncmp(entry.mount_point.c_str(),(char*)EX_SDCARD_ROOT, strlen((char*)EX_SDCARD_ROOT)) == 0) || (strncmp(entry.mount_point.c_str(),(char*)USB_ROOT, strlen((char*)USB_ROOT)) == 0))
      {
        continue;
      }
      if (ensure_path_unmounted(entry.mount_point) != 0) {
        LOG(ERROR) << "Failed to unmount " << entry.mount_point;
        return -1;
      }
    }
  }
  return 0;
}

bool HasCache() {
  CHECK(!fstab.empty());
  static bool has_cache = volume_for_mount_point(CACHE_ROOT) != nullptr;
  return has_cache;
}
