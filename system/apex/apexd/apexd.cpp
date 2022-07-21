/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "apexd.h"
#include "apexd_private.h"

#include "apex_constants.h"
#include "apex_database.h"
#include "apex_file.h"
#include "apex_manifest.h"
#include "apex_preinstalled_data.h"
#include "apex_shim.h"
#include "apexd_checkpoint.h"
#include "apexd_loop.h"
#include "apexd_prepostinstall.h"
#include "apexd_prop.h"
#include "apexd_rollback_utils.h"
#include "apexd_session.h"
#include "apexd_utils.h"
#include "apexd_verity.h"
#include "string_log.h"

#include <ApexProperties.sysprop.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/scopeguard.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <libavb/libavb.h>
#include <libdm/dm.h>
#include <libdm/dm_table.h>
#include <libdm/dm_target.h>
#include <selinux/android.h>

#include <dirent.h>
#include <fcntl.h>
#include <linux/loop.h>
#include <log/log.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using android::base::ErrnoError;
using android::base::Error;
using android::base::GetProperty;
using android::base::Join;
using android::base::ParseUint;
using android::base::ReadFully;
using android::base::Result;
using android::base::StartsWith;
using android::base::StringPrintf;
using android::base::unique_fd;
using android::dm::DeviceMapper;
using android::dm::DmDeviceState;
using android::dm::DmTable;
using android::dm::DmTargetVerity;

using apex::proto::SessionState;

namespace android {
namespace apex {

using MountedApexData = MountedApexDatabase::MountedApexData;

namespace {

// These should be in-sync with system/sepolicy/public/property_contexts
static constexpr const char* kApexStatusSysprop = "apexd.status";
static constexpr const char* kApexStatusStarting = "starting";
static constexpr const char* kApexStatusActivated = "activated";
static constexpr const char* kApexStatusReady = "ready";

static constexpr const char* kBuildFingerprintSysprop = "ro.build.fingerprint";

// This should be in UAPI, but it's not :-(
static constexpr const char* kDmVerityRestartOnCorruption =
    "restart_on_corruption";

MountedApexDatabase gMountedApexes;

CheckpointInterface* gVoldService;
bool gSupportsFsCheckpoints = false;
bool gInFsCheckpointMode = false;

static constexpr size_t kLoopDeviceSetupAttempts = 3u;

bool gBootstrap = false;
static const std::vector<std::string> kBootstrapApexes = ([]() {
  std::vector<std::string> ret = {
      "com.android.art",
      "com.android.i18n",
      "com.android.runtime",
      "com.android.tzdata",
      "com.android.os.statsd",
  };

  auto vendor_vndk_ver = GetProperty("ro.vndk.version", "");
  if (vendor_vndk_ver != "") {
    ret.push_back("com.android.vndk.v" + vendor_vndk_ver);
  }
  auto product_vndk_ver = GetProperty("ro.product.vndk.version", "");
  if (product_vndk_ver != "" && product_vndk_ver != vendor_vndk_ver) {
    ret.push_back("com.android.vndk.v" + product_vndk_ver);
  }
  return ret;
})();

static constexpr const int kNumRetriesWhenCheckpointingEnabled = 1;

bool isBootstrapApex(const ApexFile& apex) {
  return std::find(kBootstrapApexes.begin(), kBootstrapApexes.end(),
                   apex.GetManifest().name()) != kBootstrapApexes.end();
}

// Pre-allocate loop devices so that we don't have to wait for them
// later when actually activating APEXes.
Result<void> preAllocateLoopDevices() {
  auto scan = FindApexes(kApexPackageBuiltinDirs);
  if (!scan.ok()) {
    return scan.error();
  }

  auto size = 0;
  for (const auto& path : *scan) {
    auto apexFile = ApexFile::Open(path);
    if (!apexFile.ok()) {
      continue;
    }
    size++;
    // bootstrap Apexes may be activated on separate namespaces.
    if (isBootstrapApex(*apexFile)) {
      size++;
    }
  }

  // note: do not call preAllocateLoopDevices() if size == 0.
  // For devices (e.g. ARC) which doesn't support loop-control
  // preAllocateLoopDevices() can cause problem when it tries
  // to access /dev/loop-control.
  if (size == 0) {
    return {};
  }
  return loop::preAllocateLoopDevices(size);
}

std::unique_ptr<DmTable> createVerityTable(const ApexVerityData& verity_data,
                                           const std::string& block_device,
                                           const std::string& hash_device,
                                           bool restart_on_corruption) {
  AvbHashtreeDescriptor* desc = verity_data.desc.get();
  auto table = std::make_unique<DmTable>();

  uint32_t hash_start_block = 0;
  if (hash_device == block_device) {
    hash_start_block = desc->tree_offset / desc->hash_block_size;
  }

  auto target = std::make_unique<DmTargetVerity>(
      0, desc->image_size / 512, desc->dm_verity_version, block_device,
      hash_device, desc->data_block_size, desc->hash_block_size,
      desc->image_size / desc->data_block_size, hash_start_block,
      verity_data.hash_algorithm, verity_data.root_digest, verity_data.salt);

  target->IgnoreZeroBlocks();
  if (restart_on_corruption) {
    target->SetVerityMode(kDmVerityRestartOnCorruption);
  }
  table->AddTarget(std::move(target));

  table->set_readonly(true);

  return table;
}

// Deletes a dm-verity device with a given name and path
// Synchronizes on the device actually being deleted from userspace.
Result<void> DeleteVerityDevice(const std::string& name) {
  DeviceMapper& dm = DeviceMapper::Instance();
  if (!dm.DeleteDevice(name, 750ms)) {
    return Error() << "Failed to delete dm-device " << name;
  }
  return {};
}

class DmVerityDevice {
 public:
  DmVerityDevice() : cleared_(true) {}
  explicit DmVerityDevice(std::string name)
      : name_(std::move(name)), cleared_(false) {}
  DmVerityDevice(std::string name, std::string dev_path)
      : name_(std::move(name)),
        dev_path_(std::move(dev_path)),
        cleared_(false) {}

  DmVerityDevice(DmVerityDevice&& other) noexcept
      : name_(std::move(other.name_)),
        dev_path_(std::move(other.dev_path_)),
        cleared_(other.cleared_) {
    other.cleared_ = true;
  }

  DmVerityDevice& operator=(DmVerityDevice&& other) noexcept {
    name_ = other.name_;
    dev_path_ = other.dev_path_;
    cleared_ = other.cleared_;
    other.cleared_ = true;
    return *this;
  }

  ~DmVerityDevice() {
    if (!cleared_) {
      Result<void> ret = DeleteVerityDevice(name_);
      if (!ret.ok()) {
        LOG(ERROR) << ret.error();
      }
    }
  }

  const std::string& GetName() const { return name_; }
  const std::string& GetDevPath() const { return dev_path_; }

  void Release() { cleared_ = true; }

 private:
  std::string name_;
  std::string dev_path_;
  bool cleared_;
};

Result<DmVerityDevice> createVerityDevice(const std::string& name,
                                          const DmTable& table) {
  DeviceMapper& dm = DeviceMapper::Instance();

  if (dm.GetState(name) != DmDeviceState::INVALID) {
    // TODO: since apexd tears down devices during unmount, can this happen?
    LOG(WARNING) << "Deleting existing dm device " << name;
    const Result<void>& result = DeleteVerityDevice(name);
    if (!result.ok()) {
      // TODO: should we fail instead?
      LOG(ERROR) << "Failed to delete device " << name << " : "
                 << result.error();
    }
  }

  std::string dev_path;
  if (!dm.CreateDevice(name, table, &dev_path, 500ms)) {
    return Errorf("Couldn't create verity device.");
  }
  return DmVerityDevice(name, dev_path);
}

Result<void> RemovePreviouslyActiveApexFiles(
    const std::unordered_set<std::string>& affected_packages,
    const std::unordered_set<std::string>& files_to_keep) {
  auto all_active_apex_files = FindApexFilesByName(kActiveApexPackagesDataDir);

  if (!all_active_apex_files.ok()) {
    return all_active_apex_files.error();
  }

  for (const std::string& path : *all_active_apex_files) {
    Result<ApexFile> apex_file = ApexFile::Open(path);
    if (!apex_file.ok()) {
      return apex_file.error();
    }

    const std::string& package_name = apex_file->GetManifest().name();
    if (affected_packages.find(package_name) == affected_packages.end()) {
      // This apex belongs to a package that wasn't part of this stage sessions,
      // hence it should be kept.
      continue;
    }

    if (files_to_keep.find(apex_file->GetPath()) != files_to_keep.end()) {
      // This is a path that was staged and should be kept.
      continue;
    }

    LOG(DEBUG) << "Deleting previously active apex " << apex_file->GetPath();
    if (unlink(apex_file->GetPath().c_str()) != 0) {
      return ErrnoError() << "Failed to unlink " << apex_file->GetPath();
    }
  }

  return {};
}

// Reads the entire device to verify the image is authenticatic
Result<void> readVerityDevice(const std::string& verity_device,
                              uint64_t device_size) {
  static constexpr int kBlockSize = 4096;
  static constexpr size_t kBufSize = 1024 * kBlockSize;
  std::vector<uint8_t> buffer(kBufSize);

  unique_fd fd(TEMP_FAILURE_RETRY(open(verity_device.c_str(), O_RDONLY)));
  if (fd.get() == -1) {
    return ErrnoError() << "Can't open " << verity_device;
  }

  size_t bytes_left = device_size;
  while (bytes_left > 0) {
    size_t to_read = std::min(bytes_left, kBufSize);
    if (!android::base::ReadFully(fd.get(), buffer.data(), to_read)) {
      return ErrnoError() << "Can't verify " << verity_device << "; corrupted?";
    }
    bytes_left -= to_read;
  }

  return {};
}

Result<void> VerifyMountedImage(const ApexFile& apex,
                                const std::string& mount_point) {
  auto result = apex.VerifyManifestMatches(mount_point);
  if (!result.ok()) {
    return result;
  }
  if (shim::IsShimApex(apex)) {
    return shim::ValidateShimApex(mount_point, apex);
  }
  return {};
}

Result<MountedApexData> MountPackageImpl(const ApexFile& apex,
                                         const std::string& mountPoint,
                                         const std::string& device_name,
                                         const std::string& hashtree_file,
                                         bool verifyImage) {
  LOG(VERBOSE) << "Creating mount point: " << mountPoint;
  // Note: the mount point could exist in case when the APEX was activated
  // during the bootstrap phase (e.g., the runtime or tzdata APEX).
  // Although we have separate mount namespaces to separate the early activated
  // APEXes from the normally activate APEXes, the mount points themselves
  // are shared across the two mount namespaces because /apex (a tmpfs) itself
  // mounted at / which is (and has to be) a shared mount. Therefore, if apexd
  // finds an empty directory under /apex, it's not a problem and apexd can use
  // it.
  auto exists = PathExists(mountPoint);
  if (!exists.ok()) {
    return exists.error();
  }
  if (!*exists && mkdir(mountPoint.c_str(), kMkdirMode) != 0) {
    return ErrnoError() << "Could not create mount point " << mountPoint;
  }
  auto deleter = [&mountPoint]() {
    if (rmdir(mountPoint.c_str()) != 0) {
      PLOG(WARNING) << "Could not rmdir " << mountPoint;
    }
  };
  auto scope_guard = android::base::make_scope_guard(deleter);
  if (!IsEmptyDirectory(mountPoint)) {
    return ErrnoError() << mountPoint << " is not empty";
  }

  const std::string& full_path = apex.GetPath();

  loop::LoopbackDeviceUniqueFd loopbackDevice;
  for (size_t attempts = 1;; ++attempts) {
    Result<loop::LoopbackDeviceUniqueFd> ret = loop::createLoopDevice(
        full_path, apex.GetImageOffset(), apex.GetImageSize());
    if (ret.ok()) {
      loopbackDevice = std::move(*ret);
      break;
    }
    if (attempts >= kLoopDeviceSetupAttempts) {
      return Error() << "Could not create loop device for " << full_path << ": "
                     << ret.error();
    }
  }
  LOG(VERBOSE) << "Loopback device created: " << loopbackDevice.name;

  auto verityData = apex.VerifyApexVerity();
  if (!verityData.ok()) {
    return Error() << "Failed to verify Apex Verity data for " << full_path
                   << ": " << verityData.error();
  }
  std::string blockDevice = loopbackDevice.name;
  MountedApexData apex_data(loopbackDevice.name, apex.GetPath(), mountPoint,
                            /* device_name = */ "",
                            /* hashtree_loop_name = */ "");

  // for APEXes in immutable partitions, we don't need to mount them on
  // dm-verity because they are already in the dm-verity protected partition;
  // system. However, note that we don't skip verification to ensure that APEXes
  // are correctly signed.
  const bool mountOnVerity = !isPathForBuiltinApexes(full_path);
  DmVerityDevice verityDev;
  loop::LoopbackDeviceUniqueFd loop_for_hash;
  if (mountOnVerity) {
    std::string hash_device = loopbackDevice.name;
    if (verityData->desc->tree_size == 0) {
      if (auto st = PrepareHashTree(apex, *verityData, hashtree_file);
          !st.ok()) {
        return st.error();
      }
      auto create_loop_status = loop::createLoopDevice(hashtree_file, 0, 0);
      if (!create_loop_status.ok()) {
        return create_loop_status.error();
      }
      loop_for_hash = std::move(*create_loop_status);
      hash_device = loop_for_hash.name;
      apex_data.hashtree_loop_name = hash_device;
    }
    auto verityTable =
        createVerityTable(*verityData, loopbackDevice.name, hash_device,
                          /* restart_on_corruption = */ !verifyImage);
    Result<DmVerityDevice> verityDevRes =
        createVerityDevice(device_name, *verityTable);
    if (!verityDevRes.ok()) {
      return Error() << "Failed to create Apex Verity device " << full_path
                     << ": " << verityDevRes.error();
    }
    verityDev = std::move(*verityDevRes);
    apex_data.device_name = device_name;
    blockDevice = verityDev.GetDevPath();

    Result<void> readAheadStatus =
        loop::configureReadAhead(verityDev.GetDevPath());
    if (!readAheadStatus.ok()) {
      return readAheadStatus.error();
    }
  }
  // TODO: consider moving this inside RunVerifyFnInsideTempMount.
  if (mountOnVerity && verifyImage) {
    Result<void> verityStatus =
        readVerityDevice(blockDevice, (*verityData).desc->image_size);
    if (!verityStatus.ok()) {
      return verityStatus.error();
    }
  }

  uint32_t mountFlags = MS_NOATIME | MS_NODEV | MS_DIRSYNC | MS_RDONLY;
  if (apex.GetManifest().nocode()) {
    mountFlags |= MS_NOEXEC;
  }

  if (mount(blockDevice.c_str(), mountPoint.c_str(), "ext4", mountFlags,
            nullptr) == 0) {
    LOG(INFO) << "Successfully mounted package " << full_path << " on "
              << mountPoint;
    auto status = VerifyMountedImage(apex, mountPoint);
    if (!status.ok()) {
      if (umount2(mountPoint.c_str(), UMOUNT_NOFOLLOW) != 0) {
        PLOG(ERROR) << "Failed to umount " << mountPoint;
      }
      return Error() << "Failed to verify " << full_path << ": "
                     << status.error();
    }
    // Time to accept the temporaries as good.
    verityDev.Release();
    loopbackDevice.CloseGood();
    loop_for_hash.CloseGood();

    scope_guard.Disable();  // Accept the mount.
    return apex_data;
  } else {
    return ErrnoError() << "Mounting failed for package " << full_path;
  }
}

std::string GetHashTreeFileName(const ApexFile& apex, bool is_new) {
  std::string ret =
      std::string(kApexHashTreeDir) + "/" + GetPackageId(apex.GetManifest());
  return is_new ? ret + ".new" : ret;
}

Result<MountedApexData> VerifyAndTempMountPackage(
    const ApexFile& apex, const std::string& mount_point) {
  const std::string& package_id = GetPackageId(apex.GetManifest());
  LOG(DEBUG) << "Temp mounting " << package_id << " to " << mount_point;
  const std::string& temp_device_name = package_id + ".tmp";
  std::string hashtree_file = GetHashTreeFileName(apex, /* is_new = */ true);
  if (access(hashtree_file.c_str(), F_OK) == 0) {
    LOG(DEBUG) << hashtree_file << " already exists. Deleting it";
    if (TEMP_FAILURE_RETRY(unlink(hashtree_file.c_str())) != 0) {
      return ErrnoError() << "Failed to unlink " << hashtree_file;
    }
  }
  return MountPackageImpl(apex, mount_point, temp_device_name,
                          GetHashTreeFileName(apex, /* is_new = */ true),
                          /* verifyImage = */ true);
}

Result<void> Unmount(const MountedApexData& data) {
  LOG(DEBUG) << "Unmounting " << data.full_path << " from mount point "
             << data.mount_point;
  // Lazily try to umount whatever is mounted.
  if (umount2(data.mount_point.c_str(), UMOUNT_NOFOLLOW) != 0 &&
      errno != EINVAL && errno != ENOENT) {
    return ErrnoError() << "Failed to unmount directory " << data.mount_point;
  }
  // Attempt to delete the folder. If the folder is retained, other
  // data may be incorrect.
  if (rmdir(data.mount_point.c_str()) != 0) {
    PLOG(ERROR) << "Failed to rmdir directory " << data.mount_point;
  }

  // Try to free up the device-mapper device.
  if (!data.device_name.empty()) {
    const auto& result = DeleteVerityDevice(data.device_name);
    if (!result.ok()) {
      return result;
    }
  }

  // Try to free up the loop device.
  auto log_fn = [](const std::string& path, const std::string& /*id*/) {
    LOG(VERBOSE) << "Freeing loop device " << path << " for unmount.";
  };
  if (!data.loop_name.empty()) {
    loop::DestroyLoopDevice(data.loop_name, log_fn);
  }
  if (!data.hashtree_loop_name.empty()) {
    loop::DestroyLoopDevice(data.hashtree_loop_name, log_fn);
  }

  return {};
}

template <typename VerifyFn>
Result<void> RunVerifyFnInsideTempMount(const ApexFile& apex,
                                        const VerifyFn& verify_fn) {
  // Temp mount image of this apex to validate it was properly signed;
  // this will also read the entire block device through dm-verity, so
  // we can be sure there is no corruption.
  const std::string& temp_mount_point =
      apexd_private::GetPackageTempMountPoint(apex.GetManifest());

  Result<MountedApexData> mount_status =
      VerifyAndTempMountPackage(apex, temp_mount_point);
  if (!mount_status.ok()) {
    LOG(ERROR) << "Failed to temp mount to " << temp_mount_point << " : "
               << mount_status.error();
    return mount_status.error();
  }
  auto cleaner = [&]() {
    LOG(DEBUG) << "Unmounting " << temp_mount_point;
    Result<void> result = Unmount(*mount_status);
    if (!result.ok()) {
      LOG(WARNING) << "Failed to unmount " << temp_mount_point << " : "
                   << result.error();
    }
  };
  auto scope_guard = android::base::make_scope_guard(cleaner);
  return verify_fn(temp_mount_point);
}

template <typename HookFn, typename HookCall>
Result<void> PrePostinstallPackages(const std::vector<ApexFile>& apexes,
                                    HookFn fn, HookCall call) {
  if (apexes.empty()) {
    return Errorf("Empty set of inputs");
  }

  // 1) Check whether the APEXes have hooks.
  bool has_hooks = false;
  for (const ApexFile& apex_file : apexes) {
    if (!(apex_file.GetManifest().*fn)().empty()) {
      has_hooks = true;
      break;
    }
  }

  // 2) If we found hooks, run the pre/post-install.
  if (has_hooks) {
    Result<void> install_status = (*call)(apexes);
    if (!install_status.ok()) {
      return install_status;
    }
  }

  return {};
}

Result<void> PreinstallPackages(const std::vector<ApexFile>& apexes) {
  return PrePostinstallPackages(apexes, &ApexManifest::preinstallhook,
                                &StagePreInstall);
}

Result<void> PostinstallPackages(const std::vector<ApexFile>& apexes) {
  return PrePostinstallPackages(apexes, &ApexManifest::postinstallhook,
                                &StagePostInstall);
}

template <typename RetType, typename Fn>
RetType HandlePackages(const std::vector<std::string>& paths, Fn fn) {
  // 1) Open all APEXes.
  std::vector<ApexFile> apex_files;
  for (const std::string& path : paths) {
    Result<ApexFile> apex_file = ApexFile::Open(path);
    if (!apex_file.ok()) {
      return apex_file.error();
    }
    apex_files.emplace_back(std::move(*apex_file));
  }

  // 2) Dispatch.
  return fn(apex_files);
}

Result<void> ValidateStagingShimApex(const ApexFile& to) {
  using android::base::StringPrintf;
  auto system_shim = ApexFile::Open(
      StringPrintf("%s/%s", kApexPackageSystemDir, shim::kSystemShimApexName));
  if (!system_shim.ok()) {
    return system_shim.error();
  }
  auto verify_fn = [&](const std::string& system_apex_path) {
    return shim::ValidateUpdate(system_apex_path, to.GetPath());
  };
  return RunVerifyFnInsideTempMount(*system_shim, verify_fn);
}

// A version of apex verification that happens during boot.
// This function should only verification checks that are necessary to run on
// each boot. Try to avoid putting expensive checks inside this function.
Result<void> VerifyPackageBoot(const ApexFile& apex_file) {
  Result<ApexVerityData> verity_or = apex_file.VerifyApexVerity();
  if (!verity_or.ok()) {
    return verity_or.error();
  }

  if (shim::IsShimApex(apex_file)) {
    // Validating shim is not a very cheap operation, but it's fine to perform
    // it here since it only runs during CTS tests and will never be triggered
    // during normal flow.
    const auto& result = ValidateStagingShimApex(apex_file);
    if (!result.ok()) {
      return result;
    }
  }
  return {};
}

// A version of apex verification that happens on submitStagedSession.
// This function contains checks that might be expensive to perform, e.g. temp
// mounting a package and reading entire dm-verity device, and shouldn't be run
// during boot.
Result<void> VerifyPackageInstall(const ApexFile& apex_file) {
  const auto& verify_package_boot_status = VerifyPackageBoot(apex_file);
  if (!verify_package_boot_status.ok()) {
    return verify_package_boot_status;
  }
  Result<ApexVerityData> verity_or = apex_file.VerifyApexVerity();

  constexpr const auto kSuccessFn = [](const std::string& /*mount_point*/) {
    return Result<void>{};
  };
  return RunVerifyFnInsideTempMount(apex_file, kSuccessFn);
}

template <typename VerifyApexFn>
Result<std::vector<ApexFile>> verifyPackages(
    const std::vector<std::string>& paths, const VerifyApexFn& verify_apex_fn) {
  if (paths.empty()) {
    return Errorf("Empty set of inputs");
  }
  LOG(DEBUG) << "verifyPackages() for " << Join(paths, ',');

  auto verify_fn = [&](std::vector<ApexFile>& apexes) {
    for (const ApexFile& apex_file : apexes) {
      Result<void> result = verify_apex_fn(apex_file);
      if (!result.ok()) {
        return Result<std::vector<ApexFile>>(result.error());
      }
    }
    return Result<std::vector<ApexFile>>(std::move(apexes));
  };
  return HandlePackages<Result<std::vector<ApexFile>>>(paths, verify_fn);
}

Result<ApexFile> verifySessionDir(const int session_id) {
  std::string sessionDirPath = std::string(kStagedSessionsDir) + "/session_" +
                               std::to_string(session_id);
  LOG(INFO) << "Scanning " << sessionDirPath
            << " looking for packages to be validated";
  Result<std::vector<std::string>> scan = FindApexFilesByName(sessionDirPath);
  if (!scan.ok()) {
    LOG(WARNING) << scan.error();
    return scan.error();
  }

  if (scan->size() > 1) {
    return Errorf(
        "More than one APEX package found in the same session directory.");
  }

  auto verified = verifyPackages(*scan, VerifyPackageInstall);
  if (!verified.ok()) {
    return verified.error();
  }
  return std::move((*verified)[0]);
}

Result<void> DeleteBackup() {
  auto exists = PathExists(std::string(kApexBackupDir));
  if (!exists.ok()) {
    return Error() << "Can't clean " << kApexBackupDir << " : "
                   << exists.error();
  }
  if (!*exists) {
    LOG(DEBUG) << kApexBackupDir << " does not exist. Nothing to clean";
    return {};
  }
  return DeleteDirContent(std::string(kApexBackupDir));
}

Result<void> BackupActivePackages() {
  LOG(DEBUG) << "Initializing  backup of " << kActiveApexPackagesDataDir;

  // Previous restore might've delete backups folder.
  auto create_status = createDirIfNeeded(kApexBackupDir, 0700);
  if (!create_status.ok()) {
    return Error() << "Backup failed : " << create_status.error();
  }

  auto apex_active_exists = PathExists(std::string(kActiveApexPackagesDataDir));
  if (!apex_active_exists.ok()) {
    return Error() << "Backup failed : " << apex_active_exists.error();
  }
  if (!*apex_active_exists) {
    LOG(DEBUG) << kActiveApexPackagesDataDir
               << " does not exist. Nothing to backup";
    return {};
  }

  auto active_packages = FindApexFilesByName(kActiveApexPackagesDataDir);
  if (!active_packages.ok()) {
    return Error() << "Backup failed : " << active_packages.error();
  }

  auto cleanup_status = DeleteBackup();
  if (!cleanup_status.ok()) {
    return Error() << "Backup failed : " << cleanup_status.error();
  }

  auto backup_path_fn = [](const ApexFile& apex_file) {
    return StringPrintf("%s/%s%s", kApexBackupDir,
                        GetPackageId(apex_file.GetManifest()).c_str(),
                        kApexPackageSuffix);
  };

  auto deleter = []() {
    auto result = DeleteDirContent(std::string(kApexBackupDir));
    if (!result.ok()) {
      LOG(ERROR) << "Failed to cleanup " << kApexBackupDir << " : "
                 << result.error();
    }
  };
  auto scope_guard = android::base::make_scope_guard(deleter);

  for (const std::string& path : *active_packages) {
    Result<ApexFile> apex_file = ApexFile::Open(path);
    if (!apex_file.ok()) {
      return Error() << "Backup failed : " << apex_file.error();
    }
    const auto& dest_path = backup_path_fn(*apex_file);
    if (link(apex_file->GetPath().c_str(), dest_path.c_str()) != 0) {
      return ErrnoError() << "Failed to backup " << apex_file->GetPath();
    }
  }

  scope_guard.Disable();  // Accept the backup.
  return {};
}

Result<void> RestoreActivePackages() {
  LOG(DEBUG) << "Initializing  restore of " << kActiveApexPackagesDataDir;

  auto backup_exists = PathExists(std::string(kApexBackupDir));
  if (!backup_exists.ok()) {
    return backup_exists.error();
  }
  if (!*backup_exists) {
    return Error() << kApexBackupDir << " does not exist";
  }

  struct stat stat_data;
  if (stat(kActiveApexPackagesDataDir, &stat_data) != 0) {
    return ErrnoError() << "Failed to access " << kActiveApexPackagesDataDir;
  }

  LOG(DEBUG) << "Deleting existing packages in " << kActiveApexPackagesDataDir;
  auto delete_status =
      DeleteDirContent(std::string(kActiveApexPackagesDataDir));
  if (!delete_status.ok()) {
    return delete_status;
  }

  LOG(DEBUG) << "Renaming " << kApexBackupDir << " to "
             << kActiveApexPackagesDataDir;
  if (rename(kApexBackupDir, kActiveApexPackagesDataDir) != 0) {
    return ErrnoError() << "Failed to rename " << kApexBackupDir << " to "
                        << kActiveApexPackagesDataDir;
  }

  LOG(DEBUG) << "Restoring original permissions for "
             << kActiveApexPackagesDataDir;
  if (chmod(kActiveApexPackagesDataDir, stat_data.st_mode & ALLPERMS) != 0) {
    // TODO: should we wipe out /data/apex/active if chmod fails?
    return ErrnoError() << "Failed to restore original permissions for "
                        << kActiveApexPackagesDataDir;
  }

  return {};
}

Result<void> UnmountPackage(const ApexFile& apex, bool allow_latest) {
  LOG(VERBOSE) << "Unmounting " << GetPackageId(apex.GetManifest());

  const ApexManifest& manifest = apex.GetManifest();

  std::optional<MountedApexData> data;
  bool latest = false;

  auto fn = [&](const MountedApexData& d, bool l) {
    if (d.full_path == apex.GetPath()) {
      data.emplace(d);
      latest = l;
    }
  };
  gMountedApexes.ForallMountedApexes(manifest.name(), fn);

  if (!data) {
    return Error() << "Did not find " << apex.GetPath();
  }

  if (latest) {
    if (!allow_latest) {
      return Error() << "Package " << apex.GetPath() << " is active";
    }
    std::string mount_point = apexd_private::GetActiveMountPoint(manifest);
    LOG(VERBOSE) << "Unmounting and deleting " << mount_point;
    if (umount2(mount_point.c_str(), UMOUNT_NOFOLLOW) != 0) {
      return ErrnoError() << "Failed to unmount " << mount_point;
    }
    if (rmdir(mount_point.c_str()) != 0) {
      PLOG(ERROR) << "Could not rmdir " << mount_point;
      // Continue here.
    }
  }

  // Clean up gMountedApexes now, even though we're not fully done.
  gMountedApexes.RemoveMountedApex(manifest.name(), apex.GetPath());
  return Unmount(*data);
}

}  // namespace

Result<void> MountPackage(const ApexFile& apex, const std::string& mountPoint) {
  auto ret =
      MountPackageImpl(apex, mountPoint, GetPackageId(apex.GetManifest()),
                       GetHashTreeFileName(apex, /* is_new = */ false),
                       /* verifyImage = */ false);
  if (!ret.ok()) {
    return ret.error();
  }

  gMountedApexes.AddMountedApex(apex.GetManifest().name(), false, *ret);
  return {};
}

namespace apexd_private {

Result<MountedApexData> TempMountPackage(const ApexFile& apex,
                                         const std::string& mount_point) {
  // TODO(ioffe): consolidate these two methods.
  return android::apex::VerifyAndTempMountPackage(apex, mount_point);
}

Result<void> Unmount(const MountedApexData& data) {
  // TODO(ioffe): consolidate these two methods.
  return android::apex::Unmount(data);
}

bool IsMounted(const std::string& full_path) {
  bool found_mounted = false;
  gMountedApexes.ForallMountedApexes([&](const std::string&,
                                         const MountedApexData& data,
                                         [[maybe_unused]] bool latest) {
    if (full_path == data.full_path) {
      found_mounted = true;
    }
  });
  return found_mounted;
}

std::string GetPackageMountPoint(const ApexManifest& manifest) {
  return StringPrintf("%s/%s", kApexRoot, GetPackageId(manifest).c_str());
}

std::string GetPackageTempMountPoint(const ApexManifest& manifest) {
  return StringPrintf("%s.tmp", GetPackageMountPoint(manifest).c_str());
}

std::string GetActiveMountPoint(const ApexManifest& manifest) {
  return StringPrintf("%s/%s", kApexRoot, manifest.name().c_str());
}

}  // namespace apexd_private

Result<void> resumeRevertIfNeeded() {
  auto sessions =
      ApexSession::GetSessionsInState(SessionState::REVERT_IN_PROGRESS);
  if (sessions.empty()) {
    return {};
  }
  return revertActiveSessions("");
}

Result<void> activatePackageImpl(const ApexFile& apex_file) {
  const ApexManifest& manifest = apex_file.GetManifest();

  if (gBootstrap && !isBootstrapApex(apex_file)) {
    return {};
  }

  // See whether we think it's active, and do not allow to activate the same
  // version. Also detect whether this is the highest version.
  // We roll this into a single check.
  bool is_newest_version = true;
  bool found_other_version = false;
  bool version_found_mounted = false;
  {
    uint64_t new_version = manifest.version();
    bool version_found_active = false;
    gMountedApexes.ForallMountedApexes(
        manifest.name(), [&](const MountedApexData& data, bool latest) {
          Result<ApexFile> otherApex = ApexFile::Open(data.full_path);
          if (!otherApex.ok()) {
            return;
          }
          found_other_version = true;
          if (static_cast<uint64_t>(otherApex->GetManifest().version()) ==
              new_version) {
            version_found_mounted = true;
            version_found_active = latest;
          }
          if (static_cast<uint64_t>(otherApex->GetManifest().version()) >
              new_version) {
            is_newest_version = false;
          }
        });
    if (version_found_active) {
      LOG(DEBUG) << "Package " << manifest.name() << " with version "
                 << manifest.version() << " already active";
      return {};
    }
  }

  const std::string& mountPoint = apexd_private::GetPackageMountPoint(manifest);

  if (!version_found_mounted) {
    auto mountStatus = MountPackage(apex_file, mountPoint);
    if (!mountStatus.ok()) {
      return mountStatus;
    }
  }

  bool mounted_latest = false;
  if (is_newest_version) {
    const Result<void>& update_st = apexd_private::BindMount(
        apexd_private::GetActiveMountPoint(manifest), mountPoint);
    mounted_latest = update_st.has_value();
    if (!update_st.ok()) {
      return Error() << "Failed to update package " << manifest.name()
                     << " to version " << manifest.version() << " : "
                     << update_st.error();
    }
  }
  if (mounted_latest) {
    gMountedApexes.SetLatest(manifest.name(), apex_file.GetPath());
  }

  LOG(DEBUG) << "Successfully activated " << apex_file.GetPath()
             << " package_name: " << manifest.name()
             << " version: " << manifest.version();
  return {};
}

Result<void> activatePackage(const std::string& full_path) {
  LOG(INFO) << "Trying to activate " << full_path;

  Result<ApexFile> apex_file = ApexFile::Open(full_path);
  if (!apex_file.ok()) {
    return apex_file.error();
  }
  return activatePackageImpl(*apex_file);
}

Result<void> deactivatePackage(const std::string& full_path) {
  LOG(INFO) << "Trying to deactivate " << full_path;

  Result<ApexFile> apexFile = ApexFile::Open(full_path);
  if (!apexFile.ok()) {
    return apexFile.error();
  }

  return UnmountPackage(*apexFile, /* allow_latest= */ true);
}

std::vector<ApexFile> getActivePackages() {
  std::vector<ApexFile> ret;
  gMountedApexes.ForallMountedApexes(
      [&](const std::string&, const MountedApexData& data, bool latest) {
        if (!latest) {
          return;
        }

        Result<ApexFile> apexFile = ApexFile::Open(data.full_path);
        if (!apexFile.ok()) {
          // TODO: Fail?
          return;
        }
        ret.emplace_back(std::move(*apexFile));
      });

  return ret;
}

namespace {
std::unordered_map<std::string, uint64_t> GetActivePackagesMap() {
  std::vector<ApexFile> active_packages = getActivePackages();
  std::unordered_map<std::string, uint64_t> ret;
  for (const auto& package : active_packages) {
    const ApexManifest& manifest = package.GetManifest();
    ret.insert({manifest.name(), manifest.version()});
  }
  return ret;
}

}  // namespace

std::vector<ApexFile> getFactoryPackages() {
  std::vector<ApexFile> ret;
  for (const auto& dir : kApexPackageBuiltinDirs) {
    auto apex_files = FindApexFilesByName(dir);
    if (!apex_files.ok()) {
      LOG(ERROR) << apex_files.error();
      continue;
    }
    for (const std::string& path : *apex_files) {
      Result<ApexFile> apex_file = ApexFile::Open(path);
      if (!apex_file.ok()) {
        LOG(ERROR) << apex_file.error();
      } else {
        ret.emplace_back(std::move(*apex_file));
      }
    }
  }
  return ret;
}

Result<ApexFile> getActivePackage(const std::string& packageName) {
  std::vector<ApexFile> packages = getActivePackages();
  for (ApexFile& apex : packages) {
    if (apex.GetManifest().name() == packageName) {
      return std::move(apex);
    }
  }

  return ErrnoError() << "Cannot find matching package for: " << packageName;
}

/**
 * Abort individual staged session.
 *
 * Returns without error only if session was successfully aborted.
 **/
Result<void> abortStagedSession(int session_id) {
  auto session = ApexSession::GetSession(session_id);
  if (!session.ok()) {
    return Error() << "No session found with id " << session_id;
  }
  switch (session->GetState()) {
    case SessionState::VERIFIED:
      [[clang::fallthrough]];
    case SessionState::STAGED:
      return session->DeleteSession();
    default:
      return Error() << "Session " << *session << " can't be aborted";
  }
}

// TODO(ioffe): cleanup activation logic to avoid unnecessary scanning.
namespace {

Result<std::vector<ApexFile>> ScanApexFiles(const char* apex_package_dir) {
  LOG(INFO) << "Scanning " << apex_package_dir << " looking for APEX packages.";
  if (access(apex_package_dir, F_OK) != 0 && errno == ENOENT) {
    LOG(INFO) << "... does not exist. Skipping";
    return {};
  }
  Result<std::vector<std::string>> scan = FindApexFilesByName(apex_package_dir);
  if (!scan.ok()) {
    return Error() << "Failed to scan " << apex_package_dir << " : "
                   << scan.error();
  }
  std::vector<ApexFile> ret;
  for (const auto& name : *scan) {
    LOG(INFO) << "Found " << name;
    Result<ApexFile> apex_file = ApexFile::Open(name);
    if (!apex_file.ok()) {
      LOG(ERROR) << "Failed to scan " << name << " : " << apex_file.error();
    } else {
      ret.emplace_back(std::move(*apex_file));
    }
  }
  return ret;
}

Result<void> ActivateApexPackages(const std::vector<ApexFile>& apexes) {
  const auto& packages_with_code = GetActivePackagesMap();
  size_t failed_cnt = 0;
  size_t skipped_cnt = 0;
  size_t activated_cnt = 0;
  for (const auto& apex : apexes) {
    uint64_t new_version = static_cast<uint64_t>(apex.GetManifest().version());
    const auto& it = packages_with_code.find(apex.GetManifest().name());
    if (it != packages_with_code.end() && it->second >= new_version) {
      LOG(INFO) << "Skipping activation of " << apex.GetPath()
                << " same package with higher version " << it->second
                << " is already active";
      skipped_cnt++;
      continue;
    }

    if (auto res = activatePackageImpl(apex); !res.ok()) {
      LOG(ERROR) << "Failed to activate " << apex.GetPath() << " : "
                 << res.error();
      failed_cnt++;
    } else {
      activated_cnt++;
    }
  }
  if (failed_cnt > 0) {
    return Error() << "Failed to activate " << failed_cnt << " APEX packages";
  }
  LOG(INFO) << "Activated " << activated_cnt
            << " packages. Skipped: " << skipped_cnt;
  return {};
}

bool ShouldActivateApexOnData(const ApexFile& apex) {
  return HasPreInstalledVersion(apex.GetManifest().name());
}

}  // namespace

Result<void> scanPackagesDirAndActivate(const char* apex_package_dir) {
  auto apexes = ScanApexFiles(apex_package_dir);
  if (!apexes) {
    return apexes.error();
  }
  return ActivateApexPackages(*apexes);
}

/**
 * Snapshots data from base_dir/apexdata/<apex name> to
 * base_dir/apexrollback/<rollback id>/<apex name>.
 */
Result<void> snapshotDataDirectory(const std::string& base_dir,
                                   const int rollback_id,
                                   const std::string& apex_name,
                                   bool pre_restore = false) {
  auto rollback_path =
      StringPrintf("%s/%s/%d%s", base_dir.c_str(), kApexSnapshotSubDir,
                   rollback_id, pre_restore ? kPreRestoreSuffix : "");
  const Result<void> result = createDirIfNeeded(rollback_path, 0700);
  if (!result.ok()) {
    return Error() << "Failed to create snapshot directory for rollback "
                   << rollback_id << " : " << result.error();
  }
  auto from_path = StringPrintf("%s/%s/%s", base_dir.c_str(), kApexDataSubDir,
                                apex_name.c_str());
  auto to_path =
      StringPrintf("%s/%s", rollback_path.c_str(), apex_name.c_str());

  return ReplaceFiles(from_path, to_path);
}

/**
 * Restores snapshot from base_dir/apexrollback/<rollback id>/<apex name>
 * to base_dir/apexdata/<apex name>.
 * Note the snapshot will be deleted after restoration succeeded.
 */
Result<void> restoreDataDirectory(const std::string& base_dir,
                                  const int rollback_id,
                                  const std::string& apex_name,
                                  bool pre_restore = false) {
  auto from_path = StringPrintf(
      "%s/%s/%d%s/%s", base_dir.c_str(), kApexSnapshotSubDir, rollback_id,
      pre_restore ? kPreRestoreSuffix : "", apex_name.c_str());
  auto to_path = StringPrintf("%s/%s/%s", base_dir.c_str(), kApexDataSubDir,
                              apex_name.c_str());
  Result<void> result = ReplaceFiles(from_path, to_path);
  if (!result.ok()) {
    return result;
  }
  result = RestoreconPath(to_path);
  if (!result.ok()) {
    return result;
  }
  result = DeleteDir(from_path);
  if (!result.ok()) {
    LOG(ERROR) << "Failed to delete the snapshot: " << result.error();
  }
  return {};
}

void snapshotOrRestoreDeIfNeeded(const std::string& base_dir,
                                 const ApexSession& session) {
  if (session.HasRollbackEnabled()) {
    for (const auto& apex_name : session.GetApexNames()) {
      Result<void> result =
          snapshotDataDirectory(base_dir, session.GetRollbackId(), apex_name);
      if (!result) {
        LOG(ERROR) << "Snapshot failed for " << apex_name << ": "
                   << result.error();
      }
    }
  } else if (session.IsRollback()) {
    for (const auto& apex_name : session.GetApexNames()) {
      if (!gSupportsFsCheckpoints) {
        // Snapshot before restore so this rollback can be reverted.
        snapshotDataDirectory(base_dir, session.GetRollbackId(), apex_name,
                              true /* pre_restore */);
      }
      Result<void> result =
          restoreDataDirectory(base_dir, session.GetRollbackId(), apex_name);
      if (!result.ok()) {
        LOG(ERROR) << "Restore of data failed for " << apex_name << ": "
                   << result.error();
      }
    }
  }
}

void snapshotOrRestoreDeSysData() {
  auto sessions = ApexSession::GetSessionsInState(SessionState::ACTIVATED);

  for (const ApexSession& session : sessions) {
    snapshotOrRestoreDeIfNeeded(kDeSysDataDir, session);
  }
}

int snapshotOrRestoreDeUserData() {
  auto user_dirs = GetDeUserDirs();

  if (!user_dirs) {
    LOG(ERROR) << "Error reading dirs " << user_dirs.error();
    return 1;
  }

  auto sessions = ApexSession::GetSessionsInState(SessionState::ACTIVATED);

  for (const ApexSession& session : sessions) {
    for (const auto& user_dir : *user_dirs) {
      snapshotOrRestoreDeIfNeeded(user_dir, session);
    }
  }

  return 0;
}

Result<ino_t> snapshotCeData(const int user_id, const int rollback_id,
                             const std::string& apex_name) {
  auto base_dir = StringPrintf("%s/%d", kCeDataDir, user_id);
  Result<void> result = snapshotDataDirectory(base_dir, rollback_id, apex_name);
  if (!result) {
    return result.error();
  }
  auto ce_snapshot_path =
      StringPrintf("%s/%s/%d/%s", base_dir.c_str(), kApexSnapshotSubDir,
                   rollback_id, apex_name.c_str());
  return get_path_inode(ce_snapshot_path);
}

Result<void> restoreCeData(const int user_id, const int rollback_id,
                           const std::string& apex_name) {
  auto base_dir = StringPrintf("%s/%d", kCeDataDir, user_id);
  return restoreDataDirectory(base_dir, rollback_id, apex_name);
}

//  Migrates sessions directory from /data/apex/sessions to
//  /metadata/apex/sessions, if necessary.
Result<void> migrateSessionsDirIfNeeded() {
  return ApexSession::MigrateToMetadataSessionsDir();
}

Result<void> destroySnapshots(const std::string& base_dir,
                              const int rollback_id) {
  auto path = StringPrintf("%s/%s/%d", base_dir.c_str(), kApexSnapshotSubDir,
                           rollback_id);
  return DeleteDir(path);
}

Result<void> destroyDeSnapshots(const int rollback_id) {
  destroySnapshots(kDeSysDataDir, rollback_id);

  auto user_dirs = GetDeUserDirs();
  if (!user_dirs) {
    return Error() << "Error reading user dirs " << user_dirs.error();
  }

  for (const auto& user_dir : *user_dirs) {
    destroySnapshots(user_dir, rollback_id);
  }

  return {};
}

/**
 * Deletes all credential-encrypted snapshots for the given user, except for
 * those listed in retain_rollback_ids.
 */
Result<void> destroyCeSnapshotsNotSpecified(
    int user_id, const std::vector<int>& retain_rollback_ids) {
  auto snapshot_root =
      StringPrintf("%s/%d/%s", kCeDataDir, user_id, kApexSnapshotSubDir);
  auto snapshot_dirs = GetSubdirs(snapshot_root);
  if (!snapshot_dirs) {
    return Error() << "Error reading snapshot dirs " << snapshot_dirs.error();
  }

  for (const auto& snapshot_dir : *snapshot_dirs) {
    uint snapshot_id;
    bool parse_ok = ParseUint(
        std::filesystem::path(snapshot_dir).filename().c_str(), &snapshot_id);
    if (parse_ok &&
        std::find(retain_rollback_ids.begin(), retain_rollback_ids.end(),
                  snapshot_id) == retain_rollback_ids.end()) {
      Result<void> result = DeleteDir(snapshot_dir);
      if (!result) {
        return Error() << "Destroy CE snapshot failed for " << snapshot_dir
                       << " : " << result.error();
      }
    }
  }
  return {};
}

void restorePreRestoreSnapshotsIfPresent(const std::string& base_dir,
                                         const ApexSession& session) {
  auto pre_restore_snapshot_path =
      StringPrintf("%s/%s/%d%s", base_dir.c_str(), kApexSnapshotSubDir,
                   session.GetRollbackId(), kPreRestoreSuffix);
  if (PathExists(pre_restore_snapshot_path)) {
    for (const auto& apex_name : session.GetApexNames()) {
      Result<void> result = restoreDataDirectory(
          base_dir, session.GetRollbackId(), apex_name, true /* pre_restore */);
      if (!result) {
        LOG(ERROR) << "Restore of pre-restore snapshot failed for " << apex_name
                   << ": " << result.error();
      }
    }
  }
}

void restoreDePreRestoreSnapshotsIfPresent(const ApexSession& session) {
  restorePreRestoreSnapshotsIfPresent(kDeSysDataDir, session);

  auto user_dirs = GetDeUserDirs();
  if (!user_dirs) {
    LOG(ERROR) << "Error reading user dirs to restore pre-restore snapshots"
               << user_dirs.error();
  }

  for (const auto& user_dir : *user_dirs) {
    restorePreRestoreSnapshotsIfPresent(user_dir, session);
  }
}

void deleteDePreRestoreSnapshots(const std::string& base_dir,
                                 const ApexSession& session) {
  auto pre_restore_snapshot_path =
      StringPrintf("%s/%s/%d%s", base_dir.c_str(), kApexSnapshotSubDir,
                   session.GetRollbackId(), kPreRestoreSuffix);
  Result<void> result = DeleteDir(pre_restore_snapshot_path);
  if (!result) {
    LOG(ERROR) << "Deletion of pre-restore snapshot failed: " << result.error();
  }
}

void deleteDePreRestoreSnapshots(const ApexSession& session) {
  deleteDePreRestoreSnapshots(kDeSysDataDir, session);

  auto user_dirs = GetDeUserDirs();
  if (!user_dirs) {
    LOG(ERROR) << "Error reading user dirs to delete pre-restore snapshots"
               << user_dirs.error();
  }

  for (const auto& user_dir : *user_dirs) {
    deleteDePreRestoreSnapshots(user_dir, session);
  }
}

void scanStagedSessionsDirAndStage() {
  LOG(INFO) << "Scanning " << ApexSession::GetSessionsDir()
            << " looking for sessions to be activated.";

  auto sessionsToActivate =
      ApexSession::GetSessionsInState(SessionState::STAGED);
  if (gSupportsFsCheckpoints) {
    // A session that is in the ACTIVATED state should still be re-activated if
    // fs checkpointing is supported. In this case, a session may be in the
    // ACTIVATED state yet the data/apex/active directory may have been
    // reverted. The session should be reverted in this scenario.
    auto activatedSessions =
        ApexSession::GetSessionsInState(SessionState::ACTIVATED);
    sessionsToActivate.insert(sessionsToActivate.end(),
                              activatedSessions.begin(),
                              activatedSessions.end());
  }

  for (auto& session : sessionsToActivate) {
    auto sessionId = session.GetId();

    auto session_failed_fn = [&]() {
      LOG(WARNING) << "Marking session " << sessionId << " as failed.";
      auto st = session.UpdateStateAndCommit(SessionState::ACTIVATION_FAILED);
      if (!st.ok()) {
        LOG(WARNING) << "Failed to mark session " << sessionId
                     << " as failed : " << st.error();
      }
    };
    auto scope_guard = android::base::make_scope_guard(session_failed_fn);

    std::string build_fingerprint = GetProperty(kBuildFingerprintSysprop, "");
    if (session.GetBuildFingerprint().compare(build_fingerprint) != 0) {
      LOG(ERROR) << "APEX build fingerprint has changed";
      continue;
    }

    std::vector<std::string> dirsToScan;
    if (session.GetChildSessionIds().empty()) {
      dirsToScan.push_back(std::string(kStagedSessionsDir) + "/session_" +
                           std::to_string(sessionId));
    } else {
      for (auto childSessionId : session.GetChildSessionIds()) {
        dirsToScan.push_back(std::string(kStagedSessionsDir) + "/session_" +
                             std::to_string(childSessionId));
      }
    }

    std::vector<std::string> apexes;
    bool scanSuccessful = true;
    for (const auto& dirToScan : dirsToScan) {
      Result<std::vector<std::string>> scan = FindApexFilesByName(dirToScan);
      if (!scan.ok()) {
        LOG(WARNING) << scan.error();
        scanSuccessful = false;
        break;
      }

      if (scan->size() > 1) {
        LOG(WARNING) << "More than one APEX package found in the same session "
                     << "directory " << dirToScan << ", skipping activation.";
        scanSuccessful = false;
        break;
      }

      if (scan->empty()) {
        LOG(WARNING) << "No APEX packages found while scanning " << dirToScan
                     << " session id: " << sessionId << ".";
        scanSuccessful = false;
        break;
      }
      apexes.push_back(std::move((*scan)[0]));
    }

    if (!scanSuccessful) {
      continue;
    }

    // Run postinstall, if necessary.
    Result<void> postinstall_status = postinstallPackages(apexes);
    if (!postinstall_status.ok()) {
      LOG(ERROR) << "Postinstall failed for session "
                 << std::to_string(sessionId) << ": "
                 << postinstall_status.error();
      continue;
    }

    for (const auto& apex : apexes) {
      // TODO: Avoid opening ApexFile repeatedly.
      Result<ApexFile> apex_file = ApexFile::Open(apex);
      if (!apex_file) {
        LOG(ERROR) << "Cannot open apex file during staging: " << apex;
        continue;
      }
      session.AddApexName(apex_file->GetManifest().name());
    }

    const Result<void> result = stagePackages(apexes);
    if (!result.ok()) {
      LOG(ERROR) << "Activation failed for packages " << Join(apexes, ',')
                 << ": " << result.error();
      continue;
    }

    // Session was OK, release scopeguard.
    scope_guard.Disable();

    auto st = session.UpdateStateAndCommit(SessionState::ACTIVATED);
    if (!st.ok()) {
      LOG(ERROR) << "Failed to mark " << session
                 << " as activated : " << st.error();
    }
  }
}

Result<void> preinstallPackages(const std::vector<std::string>& paths) {
  if (paths.empty()) {
    return Errorf("Empty set of inputs");
  }
  LOG(DEBUG) << "preinstallPackages() for " << Join(paths, ',');
  return HandlePackages<Result<void>>(paths, PreinstallPackages);
}

Result<void> postinstallPackages(const std::vector<std::string>& paths) {
  if (paths.empty()) {
    return Errorf("Empty set of inputs");
  }
  LOG(DEBUG) << "postinstallPackages() for " << Join(paths, ',');
  return HandlePackages<Result<void>>(paths, PostinstallPackages);
}

namespace {
std::string StageDestPath(const ApexFile& apex_file) {
  return StringPrintf("%s/%s%s", kActiveApexPackagesDataDir,
                      GetPackageId(apex_file.GetManifest()).c_str(),
                      kApexPackageSuffix);
}

}  // namespace

Result<void> stagePackages(const std::vector<std::string>& tmpPaths) {
  if (tmpPaths.empty()) {
    return Errorf("Empty set of inputs");
  }
  LOG(DEBUG) << "stagePackages() for " << Join(tmpPaths, ',');

  // Note: this function is temporary. As such the code is not optimized, e.g.,
  //       it will open ApexFiles multiple times.

  // 1) Verify all packages.
  auto verify_status = verifyPackages(tmpPaths, VerifyPackageBoot);
  if (!verify_status.ok()) {
    return verify_status.error();
  }

  // Make sure that kActiveApexPackagesDataDir exists.
  auto create_dir_status =
      createDirIfNeeded(std::string(kActiveApexPackagesDataDir), 0755);
  if (!create_dir_status.ok()) {
    return create_dir_status.error();
  }

  // 2) Now stage all of them.

  // Ensure the APEX gets removed on failure.
  std::unordered_set<std::string> staged_files;
  std::vector<std::string> changed_hashtree_files;
  auto deleter = [&staged_files, &changed_hashtree_files]() {
    for (const std::string& staged_path : staged_files) {
      if (TEMP_FAILURE_RETRY(unlink(staged_path.c_str())) != 0) {
        PLOG(ERROR) << "Unable to unlink " << staged_path;
      }
    }
    for (const std::string& hashtree_file : changed_hashtree_files) {
      if (TEMP_FAILURE_RETRY(unlink(hashtree_file.c_str())) != 0) {
        PLOG(ERROR) << "Unable to unlink " << hashtree_file;
      }
    }
  };
  auto scope_guard = android::base::make_scope_guard(deleter);

  std::unordered_set<std::string> staged_packages;
  for (const std::string& path : tmpPaths) {
    Result<ApexFile> apex_file = ApexFile::Open(path);
    if (!apex_file.ok()) {
      return apex_file.error();
    }
    // First promote new hashtree file to the one that will be used when
    // mounting apex.
    std::string new_hashtree_file = GetHashTreeFileName(*apex_file,
                                                        /* is_new = */ true);
    std::string old_hashtree_file = GetHashTreeFileName(*apex_file,
                                                        /* is_new = */ false);
    if (access(new_hashtree_file.c_str(), F_OK) == 0) {
      if (TEMP_FAILURE_RETRY(rename(new_hashtree_file.c_str(),
                                    old_hashtree_file.c_str())) != 0) {
        return ErrnoError() << "Failed to move " << new_hashtree_file << " to "
                            << old_hashtree_file;
      }
      changed_hashtree_files.emplace_back(std::move(old_hashtree_file));
    }
    // And only then move apex to /data/apex/active.
    std::string dest_path = StageDestPath(*apex_file);
    if (access(dest_path.c_str(), F_OK) == 0) {
      LOG(DEBUG) << dest_path << " already exists. Deleting";
      if (TEMP_FAILURE_RETRY(unlink(dest_path.c_str())) != 0) {
        return ErrnoError() << "Failed to unlink " << dest_path;
      }
    }

    if (link(apex_file->GetPath().c_str(), dest_path.c_str()) != 0) {
      // TODO: Get correct binder error status.
      return ErrnoError() << "Unable to link " << apex_file->GetPath() << " to "
                          << dest_path;
    }
    staged_files.insert(dest_path);
    staged_packages.insert(apex_file->GetManifest().name());

    LOG(DEBUG) << "Success linking " << apex_file->GetPath() << " to "
               << dest_path;
  }

  scope_guard.Disable();  // Accept the state.

  return RemovePreviouslyActiveApexFiles(staged_packages, staged_files);
}

Result<void> unstagePackages(const std::vector<std::string>& paths) {
  if (paths.empty()) {
    return Errorf("Empty set of inputs");
  }
  LOG(DEBUG) << "unstagePackages() for " << Join(paths, ',');

  // TODO: to make unstage safer, we can copy to be unstaged packages to a
  // temporary folder and restore state from it in case unstagePackages fails.

  for (const std::string& path : paths) {
    if (access(path.c_str(), F_OK) != 0) {
      return ErrnoError() << "Can't access " << path;
    }
  }

  for (const std::string& path : paths) {
    if (unlink(path.c_str()) != 0) {
      return ErrnoError() << "Can't unlink " << path;
    }
  }

  return {};
}

/**
 * During apex installation, staged sessions located in /data/apex/sessions
 * mutate the active sessions in /data/apex/active. If some error occurs during
 * installation of apex, we need to revert /data/apex/active to its original
 * state and reboot.
 *
 * Also, we need to put staged sessions in /data/apex/sessions in REVERTED state
 * so that they do not get activated on next reboot.
 */
Result<void> revertActiveSessions(const std::string& crashing_native_process) {
  // First check whenever there is anything to revert. If there is none, then
  // fail. This prevents apexd from boot looping a device in case a native
  // process is crashing and there are no apex updates.
  auto activeSessions = ApexSession::GetActiveSessions();
  if (activeSessions.empty()) {
    return Error() << "Revert requested, when there are no active sessions.";
  }

  for (auto& session : activeSessions) {
    if (!crashing_native_process.empty()) {
      session.SetCrashingNativeProcess(crashing_native_process);
    }
    auto status =
        session.UpdateStateAndCommit(SessionState::REVERT_IN_PROGRESS);
    if (!status) {
      // TODO: should we continue with a revert?
      return Error() << "Revert of session " << session
                     << " failed : " << status.error();
    }
  }

  if (!gInFsCheckpointMode) {
    // SafetyNet logging for b/19393765
    android_errorWriteLog(0x534e4554, "193932765");
  }

  if (!gSupportsFsCheckpoints) {
    auto restoreStatus = RestoreActivePackages();
    if (!restoreStatus.ok()) {
      for (auto& session : activeSessions) {
        auto st = session.UpdateStateAndCommit(SessionState::REVERT_FAILED);
        LOG(DEBUG) << "Marking " << session << " as failed to revert";
        if (!st) {
          LOG(WARNING) << "Failed to mark session " << session
                       << " as failed to revert : " << st.error();
        }
      }
      return restoreStatus;
    }
  } else {
    LOG(INFO) << "Not restoring active packages in checkpoint mode.";
  }

  for (auto& session : activeSessions) {
    if (!gSupportsFsCheckpoints && session.IsRollback()) {
      // If snapshots have already been restored, undo that by restoring the
      // pre-restore snapshot.
      restoreDePreRestoreSnapshotsIfPresent(session);
    }

    auto status = session.UpdateStateAndCommit(SessionState::REVERTED);
    if (!status) {
      LOG(WARNING) << "Failed to mark session " << session
                   << " as reverted : " << status.error();
    }
  }

  return {};
}

Result<void> revertActiveSessionsAndReboot(
    const std::string& crashing_native_process) {
  auto status = revertActiveSessions(crashing_native_process);
  if (!status.ok()) {
    return status;
  }
  LOG(ERROR) << "Successfully reverted. Time to reboot device.";
  if (gInFsCheckpointMode) {
    Result<void> res = gVoldService->AbortChanges(
        "apexd_initiated" /* message */, false /* retry */);
    if (!res.ok()) {
      LOG(ERROR) << res.error();
    }
  }
  Reboot();
  return {};
}

int onBootstrap() {
  gBootstrap = true;

  Result<void> preAllocate = preAllocateLoopDevices();
  if (!preAllocate.ok()) {
    LOG(ERROR) << "Failed to pre-allocate loop devices : "
               << preAllocate.error();
  }

  std::vector<std::string> bootstrap_apex_dirs{
      kApexPackageSystemDir, kApexPackageSystemExtDir, kApexPackageVendorDir};
  Result<void> status = collectPreinstalledData(bootstrap_apex_dirs);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to collect APEX keys : " << status.error();
    return 1;
  }

  // Activate built-in APEXes for processes launched before /data is mounted.
  for (const auto& dir : bootstrap_apex_dirs) {
    auto scan_status = ScanApexFiles(dir.c_str());
    if (!scan_status.ok()) {
      LOG(ERROR) << "Failed to scan APEX files in " << dir << " : "
                 << scan_status.error();
      return 1;
    }
    if (auto ret = ActivateApexPackages(*scan_status); !ret.ok()) {
      LOG(ERROR) << "Failed to activate APEX files in " << dir << " : "
                 << ret.error();
      return 1;
    }
  }
  LOG(INFO) << "Bootstrapping done";
  return 0;
}

Result<void> remountApexFile(const std::string& path) {
  if (auto ret = deactivatePackage(path); !ret.ok()) {
    return ret;
  }
  return activatePackage(path);
}

void initializeVold(CheckpointInterface* checkpoint_service) {
  if (checkpoint_service != nullptr) {
    gVoldService = checkpoint_service;
    Result<bool> supports_fs_checkpoints =
        gVoldService->SupportsFsCheckpoints();
    if (supports_fs_checkpoints.ok()) {
      gSupportsFsCheckpoints = *supports_fs_checkpoints;
    } else {
      LOG(ERROR) << "Failed to check if filesystem checkpoints are supported: "
                 << supports_fs_checkpoints.error();
    }
    if (gSupportsFsCheckpoints) {
      Result<bool> needs_checkpoint = gVoldService->NeedsCheckpoint();
      if (needs_checkpoint.ok()) {
        gInFsCheckpointMode = *needs_checkpoint;
      } else {
        LOG(ERROR) << "Failed to check if we're in filesystem checkpoint mode: "
                   << needs_checkpoint.error();
      }
    }
  }
}

void initialize(CheckpointInterface* checkpoint_service) {
  initializeVold(checkpoint_service);
  Result<void> status = collectPreinstalledData(kApexPackageBuiltinDirs);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to collect APEX keys : " << status.error();
    return;
  }

  gMountedApexes.PopulateFromMounts();
}

void onStart() {
  LOG(INFO) << "Marking APEXd as starting";
  if (!android::base::SetProperty(kApexStatusSysprop, kApexStatusStarting)) {
    PLOG(ERROR) << "Failed to set " << kApexStatusSysprop << " to "
                << kApexStatusStarting;
  }

  // Ask whether we should revert any active sessions; this can happen if
  // we've exceeded the retry count on a device that supports filesystem
  // checkpointing.
  if (gSupportsFsCheckpoints) {
    Result<bool> needs_revert = gVoldService->NeedsRollback();
    if (!needs_revert.ok()) {
      LOG(ERROR) << "Failed to check if we need a revert: "
                 << needs_revert.error();
    } else if (*needs_revert) {
      LOG(INFO) << "Exceeded number of session retries ("
                << kNumRetriesWhenCheckpointingEnabled
                << "). Starting a revert";
      revertActiveSessions("");
    }
  }

  // Activate APEXes from /data/apex. If one in the directory is newer than the
  // system one, the new one will eclipse the old one.
  scanStagedSessionsDirAndStage();
  auto status = resumeRevertIfNeeded();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to resume revert : " << status.error();
  }

  std::vector<ApexFile> data_apex;
  if (auto scan = ScanApexFiles(kActiveApexPackagesDataDir); !scan.ok()) {
    LOG(ERROR) << "Failed to scan packages from " << kActiveApexPackagesDataDir
               << " : " << scan.error();
    if (auto revert = revertActiveSessionsAndReboot(""); !revert.ok()) {
      LOG(ERROR) << "Failed to revert : " << revert.error();
    }
  } else {
    auto filter_fn = [](const ApexFile& apex) {
      if (!ShouldActivateApexOnData(apex)) {
        LOG(WARNING) << "Skipping " << apex.GetPath();
        return false;
      }
      return true;
    };
    std::copy_if(std::make_move_iterator(scan->begin()),
                 std::make_move_iterator(scan->end()),
                 std::back_inserter(data_apex), filter_fn);
  }

  if (auto ret = ActivateApexPackages(data_apex); !ret.ok()) {
    LOG(ERROR) << "Failed to activate packages from "
               << kActiveApexPackagesDataDir << " : " << status.error();
    Result<void> revert_status = revertActiveSessionsAndReboot("");
    if (!revert_status.ok()) {
      // TODO: should we kill apexd in this case?
      LOG(ERROR) << "Failed to revert : " << revert_status.error()
                 << kActiveApexPackagesDataDir << " : " << ret.error();
    }
  }

  // Now also scan and activate APEXes from pre-installed directories.
  for (const auto& dir : kApexPackageBuiltinDirs) {
    auto scan_status = ScanApexFiles(dir.c_str());
    if (!scan_status.ok()) {
      LOG(ERROR) << "Failed to scan APEX packages from " << dir << " : "
                 << scan_status.error();
      if (auto revert = revertActiveSessionsAndReboot(""); !revert.ok()) {
        LOG(ERROR) << "Failed to revert : " << revert.error();
      }
    }
    if (auto activate = ActivateApexPackages(*scan_status); !activate.ok()) {
      // This should never happen. Like **really** never.
      // TODO: should we kill apexd in this case?
      LOG(ERROR) << "Failed to activate packages from " << dir << " : "
                 << activate.error();
    }
  }

  // Now that APEXes are mounted, snapshot or restore DE_sys data.
  snapshotOrRestoreDeSysData();
}

void onAllPackagesActivated() {
  // Set a system property to let other components know that APEXs are
  // activated, but are not yet ready to be used. init is expected to wait
  // for this status before performing configuration based on activated
  // apexes. Other components that need to use APEXs should wait for the
  // ready state instead.
  LOG(INFO) << "Marking APEXd as activated";
  if (!android::base::SetProperty(kApexStatusSysprop, kApexStatusActivated)) {
    PLOG(ERROR) << "Failed to set " << kApexStatusSysprop << " to "
                << kApexStatusActivated;
  }
}

void onAllPackagesReady() {
  // Set a system property to let other components know that APEXs are
  // correctly mounted and ready to be used. Before using any file from APEXs,
  // they can query this system property to ensure that they are okay to
  // access. Or they may have a on-property trigger to delay a task until
  // APEXs become ready.
  LOG(INFO) << "Marking APEXd as ready";
  if (!android::base::SetProperty(kApexStatusSysprop, kApexStatusReady)) {
    PLOG(ERROR) << "Failed to set " << kApexStatusSysprop << " to "
                << kApexStatusReady;
  }
}

Result<std::vector<ApexFile>> submitStagedSession(
    const int session_id, const std::vector<int>& child_session_ids,
    const bool has_rollback_enabled, const bool is_rollback,
    const int rollback_id) {
  if (session_id == 0) {
    return Error() << "Session id was not provided.";
  }

  if (!gSupportsFsCheckpoints) {
    Result<void> backup_status = BackupActivePackages();
    if (!backup_status.ok()) {
      // Do not proceed with staged install without backup
      return backup_status.error();
    }
  }

  std::vector<int> ids_to_scan;
  if (!child_session_ids.empty()) {
    ids_to_scan = child_session_ids;
  } else {
    ids_to_scan = {session_id};
  }

  std::vector<ApexFile> ret;
  for (int id_to_scan : ids_to_scan) {
    auto verified = verifySessionDir(id_to_scan);
    if (!verified.ok()) {
      return verified.error();
    }
    ret.push_back(std::move(*verified));
  }

  // Run preinstall, if necessary.
  Result<void> preinstall_status = PreinstallPackages(ret);
  if (!preinstall_status.ok()) {
    return preinstall_status.error();
  }

  if (has_rollback_enabled && is_rollback) {
    return Error() << "Cannot set session " << session_id << " as both a"
                   << " rollback and enabled for rollback.";
  }

  auto session = ApexSession::CreateSession(session_id);
  if (!session.ok()) {
    return session.error();
  }
  (*session).SetChildSessionIds(child_session_ids);
  std::string build_fingerprint = GetProperty(kBuildFingerprintSysprop, "");
  (*session).SetBuildFingerprint(build_fingerprint);
  session->SetHasRollbackEnabled(has_rollback_enabled);
  session->SetIsRollback(is_rollback);
  session->SetRollbackId(rollback_id);
  Result<void> commit_status =
      (*session).UpdateStateAndCommit(SessionState::VERIFIED);
  if (!commit_status.ok()) {
    return commit_status.error();
  }

  return ret;
}

Result<void> markStagedSessionReady(const int session_id) {
  auto session = ApexSession::GetSession(session_id);
  if (!session.ok()) {
    return session.error();
  }
  // We should only accept sessions in SessionState::VERIFIED or
  // SessionState::STAGED state. In the SessionState::STAGED case, this
  // function is effectively a no-op.
  auto session_state = (*session).GetState();
  if (session_state == SessionState::STAGED) {
    return {};
  }
  if (session_state == SessionState::VERIFIED) {
    return (*session).UpdateStateAndCommit(SessionState::STAGED);
  }
  return Error() << "Invalid state for session " << session_id
                 << ". Cannot mark it as ready.";
}

Result<void> markStagedSessionSuccessful(const int session_id) {
  auto session = ApexSession::GetSession(session_id);
  if (!session.ok()) {
    return session.error();
  }
  // Only SessionState::ACTIVATED or SessionState::SUCCESS states are accepted.
  // In the SessionState::SUCCESS state, this function is a no-op.
  if (session->GetState() == SessionState::SUCCESS) {
    return {};
  } else if (session->GetState() == SessionState::ACTIVATED) {
    auto cleanup_status = DeleteBackup();
    if (!cleanup_status.ok()) {
      return Error() << "Failed to mark session " << *session
                     << " as successful : " << cleanup_status.error();
    }
    if (session->IsRollback() && !gSupportsFsCheckpoints) {
      deleteDePreRestoreSnapshots(*session);
    }
    return session->UpdateStateAndCommit(SessionState::SUCCESS);
  } else {
    return Error() << "Session " << *session << " can not be marked successful";
  }
}

namespace {

// Find dangling mounts and unmount them.
// If one is on /data/apex/active, remove it.
void UnmountDanglingMounts() {
  std::multimap<std::string, MountedApexData> danglings;
  gMountedApexes.ForallMountedApexes([&](const std::string& package,
                                         const MountedApexData& data,
                                         bool latest) {
    if (!latest) {
      danglings.insert({package, data});
    }
  });

  for (const auto& [package, data] : danglings) {
    const std::string& path = data.full_path;
    LOG(VERBOSE) << "Unmounting " << data.mount_point;
    gMountedApexes.RemoveMountedApex(package, path);
    if (auto st = Unmount(data); !st.ok()) {
      LOG(ERROR) << st.error();
    }
    if (StartsWith(path, kActiveApexPackagesDataDir)) {
      LOG(VERBOSE) << "Deleting old APEX " << path;
      if (unlink(path.c_str()) != 0) {
        PLOG(ERROR) << "Failed to delete " << path;
      }
    }
  }

  RemoveObsoleteHashTrees();
}

// Removes APEXes on /data that don't have corresponding pre-installed version
// or that are corrupt
void RemoveOrphanedApexes() {
  auto data_apexes = FindApexFilesByName(kActiveApexPackagesDataDir);
  if (!data_apexes.ok()) {
    LOG(ERROR) << "Failed to scan " << kActiveApexPackagesDataDir << " : "
               << data_apexes.error();
    return;
  }
  for (const auto& path : *data_apexes) {
    auto apex = ApexFile::Open(path);
    if (!apex.ok()) {
      LOG(DEBUG) << "Failed to open APEX " << path << " : " << apex.error();
      // before removing, double-check if the path is active or not
      // just in case ApexFile::Open() fails with valid APEX
      if (!apexd_private::IsMounted(path)) {
        LOG(DEBUG) << "Removing corrupt APEX " << path;
        if (unlink(path.c_str()) != 0) {
          PLOG(ERROR) << "Failed to unlink " << path;
        }
      }
      continue;
    }
    if (!ShouldActivateApexOnData(*apex)) {
      LOG(DEBUG) << "Removing orphaned APEX " << path;
      if (unlink(path.c_str()) != 0) {
        PLOG(ERROR) << "Failed to unlink " << path;
      }
    }
  }
}

}  // namespace

void bootCompletedCleanup() {
  UnmountDanglingMounts();
  RemoveOrphanedApexes();
}

int unmountAll() {
  gMountedApexes.PopulateFromMounts();
  int ret = 0;
  gMountedApexes.ForallMountedApexes([&](const std::string& /*package*/,
                                         const MountedApexData& data,
                                         bool latest) {
    LOG(INFO) << "Unmounting " << data.full_path << " mounted on "
              << data.mount_point;
    if (latest) {
      auto pos = data.mount_point.find('@');
      CHECK(pos != std::string::npos);
      std::string bind_mount = data.mount_point.substr(0, pos);
      if (umount2(bind_mount.c_str(), UMOUNT_NOFOLLOW) != 0) {
        PLOG(ERROR) << "Failed to unmount bind-mount " << bind_mount;
        ret = 1;
      }
    }
    if (auto status = Unmount(data); !status.ok()) {
      LOG(ERROR) << "Failed to unmount " << data.mount_point << " : "
                 << status.error();
      ret = 1;
    }
  });
  return ret;
}

bool isBooting() {
  auto status = GetProperty(kApexStatusSysprop, "");
  return status != kApexStatusReady && status != kApexStatusActivated;
}

Result<void> remountPackages() {
  std::vector<std::string> apexes;
  gMountedApexes.ForallMountedApexes([&apexes](const std::string& /*package*/,
                                               const MountedApexData& data,
                                               bool latest) {
    if (latest) {
      LOG(DEBUG) << "Found active APEX " << data.full_path;
      apexes.push_back(data.full_path);
    }
  });
  std::vector<std::string> failed;
  for (const std::string& apex : apexes) {
    // Since this is only used during development workflow, we are trying to
    // remount as many apexes as possible instead of failing fast.
    if (auto ret = remountApexFile(apex); !ret) {
      LOG(WARNING) << "Failed to remount " << apex << " : " << ret.error();
      failed.emplace_back(apex);
    }
  }
  static constexpr const char* kErrorMessage =
      "Failed to remount following APEX packages, hence previous versions of "
      "them are still active. If APEX you are developing is in this list, it "
      "means that there still are alive processes holding a reference to the "
      "previous version of your APEX.\n";
  if (!failed.empty()) {
    return Error() << kErrorMessage << "Failed (" << failed.size() << ") "
                   << "APEX packages: [" << Join(failed, ',') << "]";
  }
  return {};
}

}  // namespace apex
}  // namespace android
