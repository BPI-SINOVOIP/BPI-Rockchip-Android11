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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <grp.h>
#include <linux/loop.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/properties.h>
#include <android-base/scopeguard.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android/os/IVold.h>
#include <binder/IServiceManager.h>
#include <fs_mgr_overlayfs.h>
#include <fstab/fstab.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libdm/dm.h>
#include <selinux/selinux.h>

#include <android/apex/ApexInfo.h>
#include <android/apex/IApexService.h>

#include "apex_constants.h"
#include "apex_database.h"
#include "apex_file.h"
#include "apex_manifest.h"
#include "apexd.h"
#include "apexd_private.h"
#include "apexd_session.h"
#include "apexd_test_utils.h"
#include "apexd_utils.h"

#include "session_state.pb.h"

using apex::proto::SessionState;

namespace android {
namespace apex {

using android::sp;
using android::String16;
using android::apex::testing::ApexInfoEq;
using android::apex::testing::CreateSessionInfo;
using android::apex::testing::IsOk;
using android::apex::testing::SessionInfoEq;
using android::base::Join;
using android::base::ReadFully;
using android::base::StartsWith;
using android::base::StringPrintf;
using android::base::unique_fd;
using android::dm::DeviceMapper;
using android::fs_mgr::Fstab;
using android::fs_mgr::GetEntryForMountPoint;
using android::fs_mgr::ReadFstabFromFile;
using ::testing::Contains;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

using MountedApexData = MountedApexDatabase::MountedApexData;

namespace fs = std::filesystem;

static void CleanDir(const std::string& dir) {
  if (access(dir.c_str(), F_OK) != 0 && errno == ENOENT) {
    LOG(WARNING) << dir << " does not exist";
    return;
  }
  auto status = WalkDir(dir, [](const fs::directory_entry& p) {
    std::error_code ec;
    fs::file_status status = p.status(ec);
    ASSERT_FALSE(ec) << "Failed to stat " << p.path() << " : " << ec.message();
    if (fs::is_directory(status)) {
      fs::remove_all(p.path(), ec);
    } else {
      fs::remove(p.path(), ec);
    }
    ASSERT_FALSE(ec) << "Failed to delete " << p.path() << " : "
                     << ec.message();
  });
  ASSERT_TRUE(IsOk(status));
}

class ApexServiceTest : public ::testing::Test {
 public:
  ApexServiceTest() {
    using android::IBinder;
    using android::IServiceManager;

    sp<IServiceManager> sm = android::defaultServiceManager();
    sp<IBinder> binder = sm->waitForService(String16("apexservice"));
    if (binder != nullptr) {
      service_ = android::interface_cast<IApexService>(binder);
    }
    binder = sm->getService(String16("vold"));
    if (binder != nullptr) {
      vold_service_ = android::interface_cast<android::os::IVold>(binder);
    }
  }

 protected:
  void SetUp() override {
    // TODO(b/136647373): Move this check to environment setup
    if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
      GTEST_SKIP() << "Skipping test because device doesn't support APEX";
    }
    ASSERT_NE(nullptr, service_.get());
    ASSERT_NE(nullptr, vold_service_.get());
    android::binder::Status status =
        vold_service_->supportsCheckpoint(&supports_fs_checkpointing_);
    ASSERT_TRUE(IsOk(status));
    CleanUp();
  }

  void TearDown() override { CleanUp(); }

  static std::string GetTestDataDir() {
    return android::base::GetExecutableDirectory();
  }
  static std::string GetTestFile(const std::string& name) {
    return GetTestDataDir() + "/" + name;
  }

  static bool HaveSelinux() { return 1 == is_selinux_enabled(); }

  static bool IsSelinuxEnforced() { return 0 != security_getenforce(); }

  Result<bool> IsActive(const std::string& name) {
    std::vector<ApexInfo> list;
    android::binder::Status status = service_->getActivePackages(&list);
    if (!status.isOk()) {
      return Error() << "Failed to check if " << name
                     << " is active : " << status.exceptionMessage().c_str();
    }
    for (const ApexInfo& apex : list) {
      if (apex.moduleName == name) {
        return true;
      }
    }
    return false;
  }

  Result<bool> IsActive(const std::string& name, int64_t version,
                        const std::string& path) {
    std::vector<ApexInfo> list;
    android::binder::Status status = service_->getActivePackages(&list);
    if (status.isOk()) {
      for (const ApexInfo& p : list) {
        if (p.moduleName == name && p.versionCode == version &&
            p.modulePath == path) {
          return true;
        }
      }
      return false;
    }
    return Error() << status.exceptionMessage().c_str();
  }

  Result<std::vector<ApexInfo>> GetAllPackages() {
    std::vector<ApexInfo> list;
    android::binder::Status status = service_->getAllPackages(&list);
    if (status.isOk()) {
      return list;
    }

    return Error() << status.toString8().c_str();
  }

  Result<std::vector<ApexInfo>> GetActivePackages() {
    std::vector<ApexInfo> list;
    android::binder::Status status = service_->getActivePackages(&list);
    if (status.isOk()) {
      return list;
    }

    return Error() << status.exceptionMessage().c_str();
  }

  Result<std::vector<ApexInfo>> GetInactivePackages() {
    std::vector<ApexInfo> list;
    android::binder::Status status = service_->getAllPackages(&list);
    list.erase(std::remove_if(
                   list.begin(), list.end(),
                   [](const ApexInfo& apexInfo) { return apexInfo.isActive; }),
               list.end());
    if (status.isOk()) {
      return list;
    }

    return Error() << status.toString8().c_str();
  }

  Result<ApexInfo> GetActivePackage(const std::string& name) {
    ApexInfo package;
    android::binder::Status status = service_->getActivePackage(name, &package);
    if (status.isOk()) {
      return package;
    }

    return Error() << status.exceptionMessage().c_str();
  }

  std::string GetPackageString(const ApexInfo& p) {
    return p.moduleName + "@" + std::to_string(p.versionCode) +
           " [path=" + p.moduleName + "]";
  }

  std::vector<std::string> GetPackagesStrings(
      const std::vector<ApexInfo>& list) {
    std::vector<std::string> ret;
    ret.reserve(list.size());
    for (const ApexInfo& p : list) {
      ret.push_back(GetPackageString(p));
    }
    return ret;
  }

  std::vector<std::string> GetActivePackagesStrings() {
    std::vector<ApexInfo> list;
    android::binder::Status status = service_->getActivePackages(&list);
    if (status.isOk()) {
      std::vector<std::string> ret(list.size());
      for (const ApexInfo& p : list) {
        ret.push_back(GetPackageString(p));
      }
      return ret;
    }

    std::vector<std::string> error;
    error.push_back("ERROR");
    return error;
  }

  Result<std::vector<ApexInfo>> GetFactoryPackages() {
    std::vector<ApexInfo> list;
    android::binder::Status status = service_->getAllPackages(&list);
    list.erase(
        std::remove_if(list.begin(), list.end(),
                       [](ApexInfo& apexInfo) { return !apexInfo.isFactory; }),
        list.end());
    if (status.isOk()) {
      return list;
    }

    return Error() << status.toString8().c_str();
  }

  static std::vector<std::string> ListDir(const std::string& path) {
    std::vector<std::string> ret;
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
      return ret;
    }
    auto status = WalkDir(path, [&](const fs::directory_entry& entry) {
      std::string tmp;
      switch (entry.symlink_status(ec).type()) {
        case fs::file_type::directory:
          tmp = "[dir]";
          break;
        case fs::file_type::symlink:
          tmp = "[lnk]";
          break;
        case fs::file_type::regular:
          tmp = "[reg]";
          break;
        default:
          tmp = "[other]";
      }
      ret.push_back(tmp.append(entry.path().filename()));
    });
    CHECK(status.has_value())
        << "Failed to list " << path << " : " << status.error();
    std::sort(ret.begin(), ret.end());
    return ret;
  }

  static std::string GetLogcat() {
    // For simplicity, log to file and read it.
    std::string file = GetTestFile("logcat.tmp.txt");
    std::vector<std::string> args{
        "/system/bin/logcat",
        "-d",
        "-f",
        file,
    };
    std::string error_msg;
    int res = ForkAndRun(args, &error_msg);
    CHECK_EQ(0, res) << error_msg;

    std::string data;
    CHECK(android::base::ReadFileToString(file, &data));

    unlink(file.c_str());

    return data;
  }

  static void DeleteIfExists(const std::string& path) {
    if (fs::exists(path)) {
      std::error_code ec;
      fs::remove_all(path, ec);
      ASSERT_FALSE(ec) << "Failed to delete dir " << path << " : "
                       << ec.message();
    }
  }

  struct PrepareTestApexForInstall {
    static constexpr const char* kTestDir = "/data/app-staging/apexservice_tmp";

    // This is given to the constructor.
    std::string test_input;           // Original test file.
    std::string selinux_label_input;  // SELinux label to apply.
    std::string test_dir_input;

    // This is derived from the input.
    std::string test_file;            // Prepared path. Under test_dir_input.
    std::string test_installed_file;  // Where apexd will store it.

    std::string package;  // APEX package name.
    uint64_t version;     // APEX version

    explicit PrepareTestApexForInstall(
        const std::string& test,
        const std::string& test_dir = std::string(kTestDir),
        const std::string& selinux_label = "staging_data_file") {
      test_input = test;
      selinux_label_input = selinux_label;
      test_dir_input = test_dir;

      test_file = test_dir_input + "/" + android::base::Basename(test);

      package = "";  // Explicitly mark as not initialized.

      Result<ApexFile> apex_file = ApexFile::Open(test);
      if (!apex_file.ok()) {
        return;
      }

      const ApexManifest& manifest = apex_file->GetManifest();
      package = manifest.name();
      version = manifest.version();

      test_installed_file = std::string(kActiveApexPackagesDataDir) + "/" +
                            package + "@" + std::to_string(version) + ".apex";
    }

    bool Prepare() {
      if (package.empty()) {
        // Failure in constructor. Redo work to get error message.
        auto fail_fn = [&]() {
          Result<ApexFile> apex_file = ApexFile::Open(test_input);
          ASSERT_FALSE(IsOk(apex_file));
          ASSERT_TRUE(apex_file.ok())
              << test_input << " failed to load: " << apex_file.error();
        };
        fail_fn();
        return false;
      }

      auto prepare = [](const std::string& src, const std::string& trg,
                        const std::string& selinux_label) {
        ASSERT_EQ(0, access(src.c_str(), F_OK))
            << src << ": " << strerror(errno);
        const std::string trg_dir = android::base::Dirname(trg);
        if (0 != mkdir(trg_dir.c_str(), 0777)) {
          int saved_errno = errno;
          ASSERT_EQ(saved_errno, EEXIST) << trg << ":" << strerror(saved_errno);
        }

        // Do not use a hardlink, even though it's the simplest solution.
        // b/119569101.
        {
          std::ifstream src_stream(src, std::ios::binary);
          ASSERT_TRUE(src_stream.good());
          std::ofstream trg_stream(trg, std::ios::binary);
          ASSERT_TRUE(trg_stream.good());

          trg_stream << src_stream.rdbuf();
        }

        ASSERT_EQ(0, chmod(trg.c_str(), 0666)) << strerror(errno);
        struct group* g = getgrnam("system");
        ASSERT_NE(nullptr, g);
        ASSERT_EQ(0, chown(trg.c_str(), /* root uid */ 0, g->gr_gid))
            << strerror(errno);

        int rc = setfilecon(
            trg_dir.c_str(),
            std::string("u:object_r:" + selinux_label + ":s0").c_str());
        ASSERT_TRUE(0 == rc || !HaveSelinux()) << strerror(errno);
        rc = setfilecon(
            trg.c_str(),
            std::string("u:object_r:" + selinux_label + ":s0").c_str());
        ASSERT_TRUE(0 == rc || !HaveSelinux()) << strerror(errno);
      };
      prepare(test_input, test_file, selinux_label_input);
      return !HasFatalFailure();
    }

    ~PrepareTestApexForInstall() {
      LOG(INFO) << "Deleting file " << test_file;
      if (unlink(test_file.c_str()) != 0) {
        PLOG(ERROR) << "Unable to unlink " << test_file;
      }
      LOG(INFO) << "Deleting directory " << test_dir_input;
      if (rmdir(test_dir_input.c_str()) != 0) {
        PLOG(ERROR) << "Unable to rmdir " << test_dir_input;
      }
    }
  };

  std::string GetDebugStr(PrepareTestApexForInstall* installer) {
    StringLog log;

    if (installer != nullptr) {
      log << "test_input=" << installer->test_input << " ";
      log << "test_file=" << installer->test_file << " ";
      log << "test_installed_file=" << installer->test_installed_file << " ";
      log << "package=" << installer->package << " ";
      log << "version=" << installer->version << " ";
    }

    log << "active=[" << Join(GetActivePackagesStrings(), ',') << "] ";
    log << kActiveApexPackagesDataDir << "=["
        << Join(ListDir(kActiveApexPackagesDataDir), ',') << "] ";
    log << kApexRoot << "=[" << Join(ListDir(kApexRoot), ',') << "]";

    return log;
  }

  sp<IApexService> service_;
  sp<android::os::IVold> vold_service_;
  bool supports_fs_checkpointing_;

 private:
  void CleanUp() {
    CleanDir(kActiveApexPackagesDataDir);
    CleanDir(kApexBackupDir);
    CleanDir(kApexHashTreeDir);
    CleanDir(ApexSession::GetSessionsDir());

    DeleteIfExists("/data/misc_ce/0/apexdata/apex.apexd_test");
    DeleteIfExists("/data/misc_ce/0/apexrollback/123456");
    DeleteIfExists("/data/misc_ce/0/apexrollback/77777");
    DeleteIfExists("/data/misc_ce/0/apexrollback/98765");
    DeleteIfExists("/data/misc_de/0/apexrollback/123456");
    DeleteIfExists("/data/misc/apexrollback/123456");
  }
};

namespace {

bool RegularFileExists(const std::string& path) {
  struct stat buf;
  if (0 != stat(path.c_str(), &buf)) {
    return false;
  }
  return S_ISREG(buf.st_mode);
}

bool DirExists(const std::string& path) {
  struct stat buf;
  if (0 != stat(path.c_str(), &buf)) {
    return false;
  }
  return S_ISDIR(buf.st_mode);
}

void CreateDir(const std::string& path) {
  std::error_code ec;
  fs::create_directory(path, ec);
  ASSERT_FALSE(ec) << "Failed to create rollback dir "
                   << " : " << ec.message();
}

void CreateFile(const std::string& path) {
  std::ofstream ofs(path);
  ASSERT_TRUE(ofs.good());
  ofs.close();
}

Result<std::vector<std::string>> ReadEntireDir(const std::string& path) {
  static const auto kAcceptAll = [](auto /*entry*/) { return true; };
  return ReadDir(path, kAcceptAll);
}

Result<std::string> GetBlockDeviceForApex(const std::string& package_id) {
  std::string mount_point = std::string(kApexRoot) + "/" + package_id;
  Fstab fstab;
  if (!ReadFstabFromFile("/proc/mounts", &fstab)) {
    return Error() << "Failed to read /proc/mounts";
  }
  auto entry = GetEntryForMountPoint(&fstab, mount_point);
  if (entry == nullptr) {
    return Error() << "Can't find " << mount_point << " in /proc/mounts";
  }
  return entry->blk_device;
}

Result<void> ReadDevice(const std::string& block_device) {
  static constexpr int kBlockSize = 4096;
  static constexpr size_t kBufSize = 1024 * kBlockSize;
  std::vector<uint8_t> buffer(kBufSize);

  unique_fd fd(TEMP_FAILURE_RETRY(open(block_device.c_str(), O_RDONLY)));
  if (fd.get() == -1) {
    return ErrnoError() << "Can't open " << block_device;
  }

  while (true) {
    int n = read(fd.get(), buffer.data(), kBufSize);
    if (n < 0) {
      return ErrnoError() << "Failed to read " << block_device;
    }
    if (n == 0) {
      break;
    }
  }
  return {};
}

std::vector<std::string> ListSlavesOfDmDevice(const std::string& name) {
  DeviceMapper& dm = DeviceMapper::Instance();
  std::string dm_path;
  EXPECT_TRUE(dm.GetDmDevicePathByName(name, &dm_path))
      << "Failed to get path of dm device " << name;
  // It's a little bit sad we can't use ConsumePrefix here :(
  constexpr std::string_view kDevPrefix = "/dev/";
  EXPECT_TRUE(StartsWith(dm_path, kDevPrefix)) << "Illegal path " << dm_path;
  dm_path = dm_path.substr(kDevPrefix.length());
  std::vector<std::string> slaves;
  {
    std::string slaves_dir = "/sys/" + dm_path + "/slaves";
    auto st = WalkDir(slaves_dir, [&](const auto& entry) {
      std::error_code ec;
      if (entry.is_symlink(ec)) {
        slaves.push_back("/dev/block/" + entry.path().filename().string());
      }
      if (ec) {
        ADD_FAILURE() << "Failed to scan " << slaves_dir << " : " << ec;
      }
    });
    EXPECT_TRUE(IsOk(st));
  }
  return slaves;
}

Result<void> CopyFile(const std::string& from, const std::string& to,
                      const fs::copy_options& options) {
  std::error_code ec;
  if (!fs::copy_file(from, to, options)) {
    return Error() << "Failed to copy file " << from << " to " << to << " : "
                   << ec.message();
  }
  return {};
}

}  // namespace

TEST_F(ApexServiceTest, HaveSelinux) {
  // We want to test under selinux.
  EXPECT_TRUE(HaveSelinux());
}

// Skip for b/119032200.
TEST_F(ApexServiceTest, DISABLED_EnforceSelinux) {
  // Crude cutout for virtual devices.
#if !defined(__i386__) && !defined(__x86_64__)
  constexpr bool kIsX86 = false;
#else
  constexpr bool kIsX86 = true;
#endif
  EXPECT_TRUE(IsSelinuxEnforced() || kIsX86);
}

TEST_F(ApexServiceTest, StageFailAccess) {
  if (!IsSelinuxEnforced()) {
    LOG(WARNING) << "Skipping InstallFailAccess because of selinux";
    return;
  }

  // Use an extra copy, so that even if this test fails (incorrectly installs),
  // we have the testdata file still around.
  std::string orig_test_file = GetTestFile("apex.apexd_test.apex");
  std::string test_file = orig_test_file + ".2";
  ASSERT_EQ(0, link(orig_test_file.c_str(), test_file.c_str()))
      << strerror(errno);
  struct Deleter {
    std::string to_delete;
    explicit Deleter(std::string t) : to_delete(std::move(t)) {}
    ~Deleter() {
      if (unlink(to_delete.c_str()) != 0) {
        PLOG(ERROR) << "Could not unlink " << to_delete;
      }
    }
  };
  Deleter del(test_file);

  android::binder::Status st = service_->stagePackages({test_file});
  ASSERT_FALSE(IsOk(st));
  std::string error = st.exceptionMessage().c_str();
  EXPECT_NE(std::string::npos, error.find("Failed to open package")) << error;
  EXPECT_NE(std::string::npos, error.find("I/O error")) << error;
}

TEST_F(ApexServiceTest, StageFailKey) {
  PrepareTestApexForInstall installer(
      GetTestFile("apex.apexd_test_no_inst_key.apex"));
  if (!installer.Prepare()) {
    return;
  }
  ASSERT_EQ(std::string("com.android.apex.test_package.no_inst_key"),
            installer.package);

  android::binder::Status st = service_->stagePackages({installer.test_file});
  ASSERT_FALSE(IsOk(st));

  // May contain one of two errors.
  std::string error = st.exceptionMessage().c_str();

  ASSERT_THAT(error, HasSubstr("No preinstalled data found for package "
                               "com.android.apex.test_package.no_inst_key"));
}

TEST_F(ApexServiceTest, StageSuccess) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"));
  if (!installer.Prepare()) {
    return;
  }
  ASSERT_EQ(std::string("com.android.apex.test_package"), installer.package);

  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
  EXPECT_TRUE(RegularFileExists(installer.test_installed_file));
}

TEST_F(ApexServiceTest,
       SubmitStagegSessionSuccessDoesNotLeakTempVerityDevices) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_1543",
                                      "staging_data_file");
  if (!installer.Prepare()) {
    return;
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 1543;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  std::vector<DeviceMapper::DmBlockDevice> devices;
  DeviceMapper& dm = DeviceMapper::Instance();
  ASSERT_TRUE(dm.GetAvailableDevices(&devices));

  for (const auto& device : devices) {
    ASSERT_THAT(device.name(), Not(EndsWith(".tmp")));
  }
}

TEST_F(ApexServiceTest, SubmitStagedSessionStoresBuildFingerprint) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_1547",
                                      "staging_data_file");
  if (!installer.Prepare()) {
    return;
  }
  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 1547;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  auto session = ApexSession::GetSession(1547);
  ASSERT_FALSE(session->GetBuildFingerprint().empty());
}

TEST_F(ApexServiceTest, SubmitStagedSessionFailDoesNotLeakTempVerityDevices) {
  PrepareTestApexForInstall installer(
      GetTestFile("apex.apexd_test_manifest_mismatch.apex"),
      "/data/app-staging/session_239", "staging_data_file");
  if (!installer.Prepare()) {
    return;
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 239;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));

  std::vector<DeviceMapper::DmBlockDevice> devices;
  DeviceMapper& dm = DeviceMapper::Instance();
  ASSERT_TRUE(dm.GetAvailableDevices(&devices));

  for (const auto& device : devices) {
    ASSERT_THAT(device.name(), Not(EndsWith(".tmp")));
  }
}

TEST_F(ApexServiceTest, StageSuccess_ClearsPreviouslyActivePackage) {
  PrepareTestApexForInstall installer1(GetTestFile("apex.apexd_test_v2.apex"));
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_different_app.apex"));
  PrepareTestApexForInstall installer3(GetTestFile("apex.apexd_test.apex"));
  auto install_fn = [&](PrepareTestApexForInstall& installer) {
    if (!installer.Prepare()) {
      return;
    }
    ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
    EXPECT_TRUE(RegularFileExists(installer.test_installed_file));
  };
  install_fn(installer1);
  install_fn(installer2);
  // Simulating a revert. After this call test_v2_apex_path should be removed.
  install_fn(installer3);

  EXPECT_FALSE(RegularFileExists(installer1.test_installed_file));
  EXPECT_TRUE(RegularFileExists(installer2.test_installed_file));
  EXPECT_TRUE(RegularFileExists(installer3.test_installed_file));
}

TEST_F(ApexServiceTest, StageAlreadyStagedPackageSuccess) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"));
  if (!installer.Prepare()) {
    return;
  }
  ASSERT_EQ(std::string("com.android.apex.test_package"), installer.package);

  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
  ASSERT_TRUE(RegularFileExists(installer.test_installed_file));

  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
  ASSERT_TRUE(RegularFileExists(installer.test_installed_file));
}

TEST_F(ApexServiceTest, StageAlreadyStagedPackageSuccessNewWins) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"));
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_nocode.apex"));
  if (!installer.Prepare() || !installer2.Prepare()) {
    return;
  }
  ASSERT_EQ(std::string("com.android.apex.test_package"), installer.package);
  ASSERT_EQ(installer.test_installed_file, installer2.test_installed_file);

  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
  const auto& apex = ApexFile::Open(installer.test_installed_file);
  ASSERT_TRUE(IsOk(apex));
  ASSERT_FALSE(apex->GetManifest().nocode());

  ASSERT_TRUE(IsOk(service_->stagePackages({installer2.test_file})));
  const auto& new_apex = ApexFile::Open(installer.test_installed_file);
  ASSERT_TRUE(IsOk(new_apex));
  ASSERT_TRUE(new_apex->GetManifest().nocode());
}

TEST_F(ApexServiceTest, MultiStageSuccess) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"));
  if (!installer.Prepare()) {
    return;
  }
  ASSERT_EQ(std::string("com.android.apex.test_package"), installer.package);

  // TODO: Add second test. Right now, just use a separate version.
  PrepareTestApexForInstall installer2(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer2.Prepare()) {
    return;
  }
  ASSERT_EQ(std::string("com.android.apex.test_package"), installer2.package);

  std::vector<std::string> packages;
  packages.push_back(installer.test_file);
  packages.push_back(installer2.test_file);

  ASSERT_TRUE(IsOk(service_->stagePackages(packages)));
  EXPECT_TRUE(RegularFileExists(installer.test_installed_file));
  EXPECT_TRUE(RegularFileExists(installer2.test_installed_file));
}

TEST_F(ApexServiceTest, CannotBeRollbackAndHaveRollbackEnabled) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_1543",
                                      "staging_data_file");
  if (!installer.Prepare()) {
    return;
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 1543;
  params.isRollback = true;
  params.hasRollbackEnabled = true;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexServiceTest, SessionParamDefaults) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_1547",
                                      "staging_data_file");
  if (!installer.Prepare()) {
    return;
  }
  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 1547;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  auto session = ApexSession::GetSession(1547);
  ASSERT_TRUE(session->GetChildSessionIds().empty());
  ASSERT_FALSE(session->IsRollback());
  ASSERT_FALSE(session->HasRollbackEnabled());
  ASSERT_EQ(0, session->GetRollbackId());
}

TEST_F(ApexServiceTest, SnapshotCeData) {
  CreateDir("/data/misc_ce/0/apexdata/apex.apexd_test");
  CreateFile("/data/misc_ce/0/apexdata/apex.apexd_test/hello.txt");

  ASSERT_TRUE(
      RegularFileExists("/data/misc_ce/0/apexdata/apex.apexd_test/hello.txt"));

  int64_t result;
  service_->snapshotCeData(0, 123456, "apex.apexd_test", &result);

  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexrollback/123456/apex.apexd_test/hello.txt"));

  // Check that the return value is the inode of the snapshot directory.
  struct stat buf;
  memset(&buf, 0, sizeof(buf));
  ASSERT_EQ(0,
            stat("/data/misc_ce/0/apexrollback/123456/apex.apexd_test", &buf));
  ASSERT_EQ(int64_t(buf.st_ino), result);
}

TEST_F(ApexServiceTest, RestoreCeData) {
  CreateDir("/data/misc_ce/0/apexdata/apex.apexd_test");
  CreateDir("/data/misc_ce/0/apexrollback/123456");
  CreateDir("/data/misc_ce/0/apexrollback/123456/apex.apexd_test");

  CreateFile("/data/misc_ce/0/apexdata/apex.apexd_test/newfile.txt");
  CreateFile("/data/misc_ce/0/apexrollback/123456/apex.apexd_test/oldfile.txt");

  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexdata/apex.apexd_test/newfile.txt"));
  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexrollback/123456/apex.apexd_test/oldfile.txt"));

  service_->restoreCeData(0, 123456, "apex.apexd_test");

  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexdata/apex.apexd_test/oldfile.txt"));
  ASSERT_FALSE(RegularFileExists(
      "/data/misc_ce/0/apexdata/apex.apexd_test/newfile.txt"));
  // The snapshot should be deleted after restoration.
  ASSERT_FALSE(
      DirExists("/data/misc_ce/0/apexrollback/123456/apex.apexd_test"));
}

TEST_F(ApexServiceTest, DestroyDeSnapshots_DeSys) {
  CreateDir("/data/misc/apexrollback/123456");
  CreateDir("/data/misc/apexrollback/123456/my.apex");
  CreateFile("/data/misc/apexrollback/123456/my.apex/hello.txt");

  ASSERT_TRUE(
      RegularFileExists("/data/misc/apexrollback/123456/my.apex/hello.txt"));

  service_->destroyDeSnapshots(8975);
  ASSERT_TRUE(
      RegularFileExists("/data/misc/apexrollback/123456/my.apex/hello.txt"));

  service_->destroyDeSnapshots(123456);
  ASSERT_FALSE(
      RegularFileExists("/data/misc/apexrollback/123456/my.apex/hello.txt"));
  ASSERT_FALSE(DirExists("/data/misc/apexrollback/123456"));
}

TEST_F(ApexServiceTest, DestroyDeSnapshots_DeUser) {
  CreateDir("/data/misc_de/0/apexrollback/123456");
  CreateDir("/data/misc_de/0/apexrollback/123456/my.apex");
  CreateFile("/data/misc_de/0/apexrollback/123456/my.apex/hello.txt");

  ASSERT_TRUE(RegularFileExists(
      "/data/misc_de/0/apexrollback/123456/my.apex/hello.txt"));

  service_->destroyDeSnapshots(8975);
  ASSERT_TRUE(RegularFileExists(
      "/data/misc_de/0/apexrollback/123456/my.apex/hello.txt"));

  service_->destroyDeSnapshots(123456);
  ASSERT_FALSE(RegularFileExists(
      "/data/misc_de/0/apexrollback/123456/my.apex/hello.txt"));
  ASSERT_FALSE(DirExists("/data/misc_de/0/apexrollback/123456"));
}

TEST_F(ApexServiceTest, DestroyCeSnapshotsNotSpecified) {
  CreateDir("/data/misc_ce/0/apexrollback/123456");
  CreateDir("/data/misc_ce/0/apexrollback/123456/apex.apexd_test");
  CreateFile("/data/misc_ce/0/apexrollback/123456/apex.apexd_test/file.txt");

  CreateDir("/data/misc_ce/0/apexrollback/77777");
  CreateDir("/data/misc_ce/0/apexrollback/77777/apex.apexd_test");
  CreateFile("/data/misc_ce/0/apexrollback/77777/apex.apexd_test/thing.txt");

  CreateDir("/data/misc_ce/0/apexrollback/98765");
  CreateDir("/data/misc_ce/0/apexrollback/98765/apex.apexd_test");
  CreateFile("/data/misc_ce/0/apexrollback/98765/apex.apexd_test/test.txt");

  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexrollback/123456/apex.apexd_test/file.txt"));
  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexrollback/77777/apex.apexd_test/thing.txt"));
  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexrollback/98765/apex.apexd_test/test.txt"));

  std::vector<int> retain{123, 77777, 987654};
  android::binder::Status st =
      service_->destroyCeSnapshotsNotSpecified(0, retain);
  ASSERT_TRUE(IsOk(st));

  ASSERT_TRUE(RegularFileExists(
      "/data/misc_ce/0/apexrollback/77777/apex.apexd_test/thing.txt"));
  ASSERT_FALSE(DirExists("/data/misc_ce/0/apexrollback/123456"));
  ASSERT_FALSE(DirExists("/data/misc_ce/0/apexrollback/98765"));
}

template <typename NameProvider>
class ApexServiceActivationTest : public ApexServiceTest {
 public:
  ApexServiceActivationTest() : stage_package(true) {}

  explicit ApexServiceActivationTest(bool stage_package)
      : stage_package(stage_package) {}

  void SetUp() override {
    // TODO(b/136647373): Move this check to environment setup
    if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
      GTEST_SKIP() << "Skipping test because device doesn't support APEX";
    }
    ApexServiceTest::SetUp();
    ASSERT_NE(nullptr, service_.get());

    installer_ = std::make_unique<PrepareTestApexForInstall>(
        GetTestFile(NameProvider::GetTestName()));
    if (!installer_->Prepare()) {
      return;
    }
    ASSERT_EQ(NameProvider::GetPackageName(), installer_->package);

    {
      // Check package is not active.
      std::string path = stage_package ? installer_->test_installed_file
                                       : installer_->test_file;
      Result<bool> active =
          IsActive(installer_->package, installer_->version, path);
      ASSERT_TRUE(IsOk(active));
      ASSERT_FALSE(*active);
    }

    if (stage_package) {
      ASSERT_TRUE(IsOk(service_->stagePackages({installer_->test_file})));
    }
  }

  void TearDown() override {
    // Attempt to deactivate.
    if (installer_ != nullptr) {
      if (stage_package) {
        service_->deactivatePackage(installer_->test_installed_file);
      } else {
        service_->deactivatePackage(installer_->test_file);
      }
    }

    installer_.reset();
    // ApexServiceTest::TearDown will wipe out everything under /data/apex.
    // Since some of that information is required for deactivatePackage binder
    // call, it's required to be called after deactivating package.
    ApexServiceTest::TearDown();
  }

  std::unique_ptr<PrepareTestApexForInstall> installer_;

 private:
  bool stage_package;
};

struct SuccessNameProvider {
  static std::string GetTestName() { return "apex.apexd_test.apex"; }
  static std::string GetPackageName() {
    return "com.android.apex.test_package";
  }
};

struct ManifestMismatchNameProvider {
  static std::string GetTestName() {
    return "apex.apexd_test_manifest_mismatch.apex";
  }
  static std::string GetPackageName() {
    return "com.android.apex.test_package";
  }
};

class ApexServiceActivationManifestMismatchFailure
    : public ApexServiceActivationTest<ManifestMismatchNameProvider> {
 public:
  ApexServiceActivationManifestMismatchFailure()
      : ApexServiceActivationTest(false) {}
};

TEST_F(ApexServiceActivationManifestMismatchFailure,
       ActivateFailsWithManifestMismatch) {
  android::binder::Status st = service_->activatePackage(installer_->test_file);
  ASSERT_FALSE(IsOk(st));

  std::string error = st.exceptionMessage().c_str();
  ASSERT_THAT(
      error,
      HasSubstr(
          "Manifest inside filesystem does not match manifest outside it"));
}

class ApexServiceActivationSuccessTest
    : public ApexServiceActivationTest<SuccessNameProvider> {};

TEST_F(ApexServiceActivationSuccessTest, Activate) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  {
    // Check package is active.
    Result<bool> active = IsActive(installer_->package, installer_->version,
                                   installer_->test_installed_file);
    ASSERT_TRUE(IsOk(active));
    ASSERT_TRUE(*active) << Join(GetActivePackagesStrings(), ',');
  }

  {
    // Check that the "latest" view exists.
    std::string latest_path =
        std::string(kApexRoot) + "/" + installer_->package;
    struct stat buf;
    ASSERT_EQ(0, stat(latest_path.c_str(), &buf)) << strerror(errno);
    // Check that it is a folder.
    EXPECT_TRUE(S_ISDIR(buf.st_mode));

    // Collect direct entries of a folder.
    auto collect_entries_fn = [&](const std::string& path) {
      std::vector<std::string> ret;
      auto status = WalkDir(path, [&](const fs::directory_entry& entry) {
        if (!entry.is_directory()) {
          return;
        }
        ret.emplace_back(entry.path().filename());
      });
      CHECK(status.has_value())
          << "Failed to list " << path << " : " << status.error();
      std::sort(ret.begin(), ret.end());
      return ret;
    };

    std::string versioned_path = std::string(kApexRoot) + "/" +
                                 installer_->package + "@" +
                                 std::to_string(installer_->version);
    std::vector<std::string> versioned_folder_entries =
        collect_entries_fn(versioned_path);
    std::vector<std::string> latest_folder_entries =
        collect_entries_fn(latest_path);

    EXPECT_TRUE(versioned_folder_entries == latest_folder_entries)
        << "Versioned: " << Join(versioned_folder_entries, ',')
        << " Latest: " << Join(latest_folder_entries, ',');
  }
}

TEST_F(ApexServiceActivationSuccessTest, GetActivePackages) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  Result<std::vector<ApexInfo>> active = GetActivePackages();
  ASSERT_TRUE(IsOk(active));
  ApexInfo match;

  for (const ApexInfo& info : *active) {
    if (info.moduleName == installer_->package) {
      match = info;
      break;
    }
  }

  ASSERT_EQ(installer_->package, match.moduleName);
  ASSERT_EQ(installer_->version, static_cast<uint64_t>(match.versionCode));
  ASSERT_EQ(installer_->test_installed_file, match.modulePath);
}

TEST_F(ApexServiceActivationSuccessTest, GetActivePackage) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  Result<ApexInfo> active = GetActivePackage(installer_->package);
  ASSERT_TRUE(IsOk(active));

  ASSERT_EQ(installer_->package, active->moduleName);
  ASSERT_EQ(installer_->version, static_cast<uint64_t>(active->versionCode));
  ASSERT_EQ(installer_->test_installed_file, active->modulePath);
}

TEST_F(ApexServiceActivationSuccessTest, ShowsUpInMountedApexDatabase) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  MountedApexDatabase db;
  db.PopulateFromMounts();

  std::optional<MountedApexData> mounted_apex;
  db.ForallMountedApexes(installer_->package,
                         [&](const MountedApexData& d, bool active) {
                           if (active) {
                             mounted_apex.emplace(d);
                           }
                         });
  ASSERT_TRUE(mounted_apex)
      << "Haven't found " << installer_->test_installed_file
      << " in the database of mounted apexes";

  // Get all necessary data for assertions on mounted_apex.
  std::string package_id =
      installer_->package + "@" + std::to_string(installer_->version);
  DeviceMapper& dm = DeviceMapper::Instance();
  std::string dm_path;
  ASSERT_TRUE(dm.GetDmDevicePathByName(package_id, &dm_path))
      << "Failed to get path of dm device " << package_id;
  auto loop_device = dm.GetParentBlockDeviceByPath(dm_path);
  ASSERT_TRUE(loop_device) << "Failed to find parent block device of "
                           << dm_path;

  // Now we are ready to assert on mounted_apex.
  ASSERT_EQ(*loop_device, mounted_apex->loop_name);
  ASSERT_EQ(installer_->test_installed_file, mounted_apex->full_path);
  std::string expected_mount = std::string(kApexRoot) + "/" + package_id;
  ASSERT_EQ(expected_mount, mounted_apex->mount_point);
  ASSERT_EQ(package_id, mounted_apex->device_name);
  ASSERT_EQ("", mounted_apex->hashtree_loop_name);
}

struct NoHashtreeApexNameProvider {
  static std::string GetTestName() {
    return "apex.apexd_test_no_hashtree.apex";
  }
  static std::string GetPackageName() {
    return "com.android.apex.test_package";
  }
};

class ApexServiceNoHashtreeApexActivationTest
    : public ApexServiceActivationTest<NoHashtreeApexNameProvider> {};

TEST_F(ApexServiceNoHashtreeApexActivationTest, Activate) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());
  {
    // Check package is active.
    Result<bool> active = IsActive(installer_->package, installer_->version,
                                   installer_->test_installed_file);
    ASSERT_TRUE(IsOk(active));
    ASSERT_TRUE(*active) << Join(GetActivePackagesStrings(), ',');
  }

  std::string package_id =
      installer_->package + "@" + std::to_string(installer_->version);
  // Check that hashtree file was created.
  {
    std::string hashtree_path =
        std::string(kApexHashTreeDir) + "/" + package_id;
    auto exists = PathExists(hashtree_path);
    ASSERT_TRUE(IsOk(exists));
    ASSERT_TRUE(*exists);
  }

  // Check that block device can be read.
  auto block_device = GetBlockDeviceForApex(package_id);
  ASSERT_TRUE(IsOk(block_device));
  ASSERT_TRUE(IsOk(ReadDevice(*block_device)));
}

TEST_F(ApexServiceNoHashtreeApexActivationTest,
       NewSessionDoesNotImpactActivePackage) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());
  {
    // Check package is active.
    Result<bool> active = IsActive(installer_->package, installer_->version,
                                   installer_->test_installed_file);
    ASSERT_TRUE(IsOk(active));
    ASSERT_TRUE(*active) << Join(GetActivePackagesStrings(), ',');
  }

  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_no_hashtree_2.apex"),
      "/data/app-staging/session_123", "staging_data_file");
  if (!installer2.Prepare()) {
    FAIL();
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 123;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  std::string package_id =
      installer_->package + "@" + std::to_string(installer_->version);
  // Check that new hashtree file was created.
  {
    std::string hashtree_path =
        std::string(kApexHashTreeDir) + "/" + package_id + ".new";
    auto exists = PathExists(hashtree_path);
    ASSERT_TRUE(IsOk(exists));
    ASSERT_TRUE(*exists) << hashtree_path << " does not exist";
  }
  // Check that active hashtree is still there.
  {
    std::string hashtree_path =
        std::string(kApexHashTreeDir) + "/" + package_id;
    auto exists = PathExists(hashtree_path);
    ASSERT_TRUE(IsOk(exists));
    ASSERT_TRUE(*exists) << hashtree_path << " does not exist";
  }

  // Check that block device of active APEX can still be read.
  auto block_device = GetBlockDeviceForApex(package_id);
  ASSERT_TRUE(IsOk(block_device));
}

TEST_F(ApexServiceNoHashtreeApexActivationTest, ShowsUpInMountedApexDatabase) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  MountedApexDatabase db;
  db.PopulateFromMounts();

  std::optional<MountedApexData> mounted_apex;
  db.ForallMountedApexes(installer_->package,
                         [&](const MountedApexData& d, bool active) {
                           if (active) {
                             mounted_apex.emplace(d);
                           }
                         });
  ASSERT_TRUE(mounted_apex)
      << "Haven't found " << installer_->test_installed_file
      << " in the database of mounted apexes";

  // Get all necessary data for assertions on mounted_apex.
  std::string package_id =
      installer_->package + "@" + std::to_string(installer_->version);
  std::vector<std::string> slaves = ListSlavesOfDmDevice(package_id);
  ASSERT_EQ(2u, slaves.size())
      << "Unexpected number of slaves: " << Join(slaves, ",");

  // Now we are ready to assert on mounted_apex.
  ASSERT_EQ(installer_->test_installed_file, mounted_apex->full_path);
  std::string expected_mount = std::string(kApexRoot) + "/" + package_id;
  ASSERT_EQ(expected_mount, mounted_apex->mount_point);
  ASSERT_EQ(package_id, mounted_apex->device_name);
  // For loops we only check that both loop_name and hashtree_loop_name are
  // slaves of the top device mapper device.
  ASSERT_THAT(slaves, Contains(mounted_apex->loop_name));
  ASSERT_THAT(slaves, Contains(mounted_apex->hashtree_loop_name));
  ASSERT_NE(mounted_apex->loop_name, mounted_apex->hashtree_loop_name);
}

TEST_F(ApexServiceNoHashtreeApexActivationTest, DeactivateFreesLoopDevices) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  std::string package_id =
      installer_->package + "@" + std::to_string(installer_->version);
  std::vector<std::string> slaves = ListSlavesOfDmDevice(package_id);
  ASSERT_EQ(2u, slaves.size())
      << "Unexpected number of slaves: " << Join(slaves, ",");

  ASSERT_TRUE(
      IsOk(service_->deactivatePackage(installer_->test_installed_file)));

  for (const auto& loop : slaves) {
    struct loop_info li;
    unique_fd fd(TEMP_FAILURE_RETRY(open(loop.c_str(), O_RDWR | O_CLOEXEC)));
    ASSERT_NE(-1, fd.get())
        << "Failed to open " << loop << " : " << strerror(errno);
    ASSERT_EQ(-1, ioctl(fd.get(), LOOP_GET_STATUS, &li))
        << loop << " is still alive";
    ASSERT_EQ(ENXIO, errno) << "Unexpected errno : " << strerror(errno);
  }

  // Skip deactivatePackage on TearDown.
  installer_.reset();
}

TEST_F(ApexServiceTest, NoHashtreeApexStagePackagesMovesHashtree) {
  PrepareTestApexForInstall installer(
      GetTestFile("apex.apexd_test_no_hashtree.apex"),
      "/data/app-staging/session_239", "staging_data_file");
  if (!installer.Prepare()) {
    FAIL();
  }

  auto read_fn = [](const std::string& path) -> std::vector<uint8_t> {
    static constexpr size_t kBufSize = 4096;
    std::vector<uint8_t> buffer(kBufSize);
    unique_fd fd(TEMP_FAILURE_RETRY(open(path.c_str(), O_RDONLY)));
    if (fd.get() == -1) {
      PLOG(ERROR) << "Failed to open " << path;
      ADD_FAILURE();
      return buffer;
    }
    if (!ReadFully(fd.get(), buffer.data(), kBufSize)) {
      PLOG(ERROR) << "Failed to read " << path;
      ADD_FAILURE();
    }
    return buffer;
  };

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 239;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  std::string package_id =
      installer.package + "@" + std::to_string(installer.version);
  // Check that new hashtree file was created.
  std::vector<uint8_t> original_hashtree_data;
  {
    std::string hashtree_path =
        std::string(kApexHashTreeDir) + "/" + package_id + ".new";
    auto exists = PathExists(hashtree_path);
    ASSERT_TRUE(IsOk(exists));
    ASSERT_TRUE(*exists);
    original_hashtree_data = read_fn(hashtree_path);
  }

  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
  // Check that hashtree file was moved.
  {
    std::string hashtree_path =
        std::string(kApexHashTreeDir) + "/" + package_id + ".new";
    auto exists = PathExists(hashtree_path);
    ASSERT_TRUE(IsOk(exists));
    ASSERT_FALSE(*exists);
  }
  {
    std::string hashtree_path =
        std::string(kApexHashTreeDir) + "/" + package_id;
    auto exists = PathExists(hashtree_path);
    ASSERT_TRUE(IsOk(exists));
    ASSERT_TRUE(*exists);
    std::vector<uint8_t> moved_hashtree_data = read_fn(hashtree_path);
    ASSERT_EQ(moved_hashtree_data, original_hashtree_data);
  }
}

TEST_F(ApexServiceTest, GetFactoryPackages) {
  Result<std::vector<ApexInfo>> factoryPackages = GetFactoryPackages();
  ASSERT_TRUE(IsOk(factoryPackages));
  ASSERT_TRUE(factoryPackages->size() > 0);

  for (const ApexInfo& package : *factoryPackages) {
    ASSERT_TRUE(isPathForBuiltinApexes(package.modulePath));
  }
}

TEST_F(ApexServiceTest, NoPackagesAreBothActiveAndInactive) {
  Result<std::vector<ApexInfo>> activePackages = GetActivePackages();
  ASSERT_TRUE(IsOk(activePackages));
  ASSERT_TRUE(activePackages->size() > 0);
  Result<std::vector<ApexInfo>> inactivePackages = GetInactivePackages();
  ASSERT_TRUE(IsOk(inactivePackages));
  std::vector<std::string> activePackagesStrings =
      GetPackagesStrings(*activePackages);
  std::vector<std::string> inactivePackagesStrings =
      GetPackagesStrings(*inactivePackages);
  std::sort(activePackagesStrings.begin(), activePackagesStrings.end());
  std::sort(inactivePackagesStrings.begin(), inactivePackagesStrings.end());
  std::vector<std::string> intersection;
  std::set_intersection(
      activePackagesStrings.begin(), activePackagesStrings.end(),
      inactivePackagesStrings.begin(), inactivePackagesStrings.end(),
      std::back_inserter(intersection));
  ASSERT_THAT(intersection, SizeIs(0));
}

TEST_F(ApexServiceTest, GetAllPackages) {
  Result<std::vector<ApexInfo>> allPackages = GetAllPackages();
  ASSERT_TRUE(IsOk(allPackages));
  ASSERT_TRUE(allPackages->size() > 0);
  Result<std::vector<ApexInfo>> activePackages = GetActivePackages();
  std::vector<std::string> activeStrings = GetPackagesStrings(*activePackages);
  Result<std::vector<ApexInfo>> factoryPackages = GetFactoryPackages();
  std::vector<std::string> factoryStrings =
      GetPackagesStrings(*factoryPackages);
  for (ApexInfo& apexInfo : *allPackages) {
    std::string packageString = GetPackageString(apexInfo);
    bool shouldBeActive = std::find(activeStrings.begin(), activeStrings.end(),
                                    packageString) != activeStrings.end();
    bool shouldBeFactory =
        std::find(factoryStrings.begin(), factoryStrings.end(),
                  packageString) != factoryStrings.end();
    ASSERT_EQ(shouldBeActive, apexInfo.isActive);
    ASSERT_EQ(shouldBeFactory, apexInfo.isFactory);
  }
}

class ApexSameGradeOfPreInstalledVersionTest : public ApexServiceTest {
 public:
  void SetUp() override {
    // TODO(b/136647373): Move this check to environment setup
    if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
      GTEST_SKIP() << "Skipping test because device doesn't support APEX";
    }
    ApexServiceTest::SetUp();
    ASSERT_NE(nullptr, service_.get());

    installer_ = std::make_unique<PrepareTestApexForInstall>(
        GetTestFile("com.android.apex.cts.shim.apex"));
    if (!installer_->Prepare()) {
      return;
    }
    ASSERT_EQ("com.android.apex.cts.shim", installer_->package);
    // First deactivate currently active shim, otherwise activatePackage will be
    // no-op.
    {
      ApexInfo system_shim;
      ASSERT_TRUE(IsOk(service_->getActivePackage("com.android.apex.cts.shim",
                                                  &system_shim)));
      ASSERT_TRUE(IsOk(service_->deactivatePackage(system_shim.modulePath)));
    }
    ASSERT_TRUE(IsOk(service_->stagePackages({installer_->test_file})));
    ASSERT_TRUE(
        IsOk(service_->activatePackage(installer_->test_installed_file)));
  }

  void TearDown() override {
    // Attempt to deactivate.
    service_->deactivatePackage(installer_->test_installed_file);
    installer_.reset();
    // ApexServiceTest::TearDown will wipe out everything under /data/apex.
    // Since some of that information is required for deactivatePackage binder
    // call, it's required to be called after deactivating package.
    ApexServiceTest::TearDown();
    ASSERT_TRUE(IsOk(service_->activatePackage(
        "/system/apex/com.android.apex.cts.shim.apex")));
  }

  std::unique_ptr<PrepareTestApexForInstall> installer_;
};

TEST_F(ApexSameGradeOfPreInstalledVersionTest, VersionOnDataWins) {
  std::vector<ApexInfo> all;
  ASSERT_TRUE(IsOk(service_->getAllPackages(&all)));

  ApexInfo on_data;
  on_data.moduleName = "com.android.apex.cts.shim";
  on_data.modulePath = "/data/apex/active/com.android.apex.cts.shim@1.apex";
  on_data.preinstalledModulePath =
      "/system/apex/com.android.apex.cts.shim.apex";
  on_data.versionCode = 1;
  on_data.isFactory = false;
  on_data.isActive = true;

  ApexInfo preinstalled;
  preinstalled.moduleName = "com.android.apex.cts.shim";
  preinstalled.modulePath = "/system/apex/com.android.apex.cts.shim.apex";
  preinstalled.preinstalledModulePath =
      "/system/apex/com.android.apex.cts.shim.apex";
  preinstalled.versionCode = 1;
  preinstalled.isFactory = true;
  preinstalled.isActive = false;

  ASSERT_THAT(all, Contains(ApexInfoEq(on_data)));
  ASSERT_THAT(all, Contains(ApexInfoEq(preinstalled)));
}

class ApexServiceDeactivationTest : public ApexServiceActivationSuccessTest {
 public:
  void SetUp() override {
    ApexServiceActivationSuccessTest::SetUp();

    ASSERT_TRUE(installer_ != nullptr);
  }

  void TearDown() override {
    installer_.reset();
    ApexServiceActivationSuccessTest::TearDown();
  }

  std::unique_ptr<PrepareTestApexForInstall> installer_;
};

TEST_F(ApexServiceActivationSuccessTest, DmDeviceTearDown) {
  std::string package_id =
      installer_->package + "@" + std::to_string(installer_->version);

  auto find_fn = [](const std::string& name) {
    auto& dm = DeviceMapper::Instance();
    std::vector<DeviceMapper::DmBlockDevice> devices;
    if (!dm.GetAvailableDevices(&devices)) {
      return Result<bool>(Errorf("GetAvailableDevices failed"));
    }
    for (const auto& device : devices) {
      if (device.name() == name) {
        return Result<bool>(true);
      }
    }
    return Result<bool>(false);
  };

#define ASSERT_FIND(type)                   \
  {                                         \
    Result<bool> res = find_fn(package_id); \
    ASSERT_RESULT_OK(res);                  \
    ASSERT_##type(*res);                    \
  }

  ASSERT_FIND(FALSE);

  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  ASSERT_FIND(TRUE);

  ASSERT_TRUE(
      IsOk(service_->deactivatePackage(installer_->test_installed_file)));

  ASSERT_FIND(FALSE);

  installer_.reset();  // Skip TearDown deactivatePackage.
}

TEST_F(ApexServiceActivationSuccessTest, DeactivateFreesLoopDevices) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  std::string package_id =
      installer_->package + "@" + std::to_string(installer_->version);
  std::vector<std::string> slaves = ListSlavesOfDmDevice(package_id);
  ASSERT_EQ(1u, slaves.size())
      << "Unexpected number of slaves: " << Join(slaves, ",");
  const std::string& loop = slaves[0];

  ASSERT_TRUE(
      IsOk(service_->deactivatePackage(installer_->test_installed_file)));

  struct loop_info li;
  unique_fd fd(TEMP_FAILURE_RETRY(open(loop.c_str(), O_RDWR | O_CLOEXEC)));
  ASSERT_NE(-1, fd.get()) << "Failed to open " << loop << " : "
                          << strerror(errno);
  ASSERT_EQ(-1, ioctl(fd.get(), LOOP_GET_STATUS, &li))
      << loop << " is still alive";
  ASSERT_EQ(ENXIO, errno) << "Unexpected errno : " << strerror(errno);

  installer_.reset();  // Skip TearDown deactivatePackage.
}

class ApexServicePrePostInstallTest : public ApexServiceTest {
 public:
  template <typename Fn>
  void RunPrePost(Fn fn, const std::vector<std::string>& apex_names,
                  const char* test_message, bool expect_success = true) {
    // Using unique_ptr is just the easiest here.
    using InstallerUPtr = std::unique_ptr<PrepareTestApexForInstall>;
    std::vector<InstallerUPtr> installers;
    std::vector<std::string> pkgs;

    for (const std::string& apex_name : apex_names) {
      InstallerUPtr installer(
          new PrepareTestApexForInstall(GetTestFile(apex_name)));
      if (!installer->Prepare()) {
        return;
      }
      pkgs.push_back(installer->test_file);
      installers.emplace_back(std::move(installer));
    }
    android::binder::Status st = (service_.get()->*fn)(pkgs);
    if (expect_success) {
      ASSERT_TRUE(IsOk(st));
    } else {
      ASSERT_FALSE(IsOk(st));
    }

    if (test_message != nullptr) {
      std::string logcat = GetLogcat();
      EXPECT_THAT(logcat, HasSubstr(test_message));
    }

    // Ensure that the package is neither active nor mounted.
    for (const InstallerUPtr& installer : installers) {
      Result<bool> active = IsActive(installer->package, installer->version,
                                     installer->test_file);
      ASSERT_TRUE(IsOk(active));
      EXPECT_FALSE(*active);
    }
    for (const InstallerUPtr& installer : installers) {
      Result<ApexFile> apex = ApexFile::Open(installer->test_input);
      ASSERT_TRUE(IsOk(apex));
      std::string path =
          apexd_private::GetPackageMountPoint(apex->GetManifest());
      std::string entry = std::string("[dir]").append(path);
      std::vector<std::string> slash_apex = ListDir(kApexRoot);
      auto it = std::find(slash_apex.begin(), slash_apex.end(), entry);
      EXPECT_TRUE(it == slash_apex.end()) << Join(slash_apex, ',');
    }
  }
};

TEST_F(ApexServicePrePostInstallTest, Preinstall) {
  RunPrePost(&IApexService::preinstallPackages,
             {"apex.apexd_test_preinstall.apex"}, "sh      : PreInstall Test");
}

TEST_F(ApexServicePrePostInstallTest, MultiPreinstall) {
  constexpr const char* kLogcatText =
      "sh      : /apex/com.android.apex.test_package/etc/sample_prebuilt_file";
  RunPrePost(&IApexService::preinstallPackages,
             {"apex.apexd_test_preinstall.apex", "apex.apexd_test.apex"},
             kLogcatText);
}

TEST_F(ApexServicePrePostInstallTest, PreinstallFail) {
  RunPrePost(&IApexService::preinstallPackages,
             {"apex.apexd_test_prepostinstall.fail.apex"},
             /* test_message= */ nullptr, /* expect_success= */ false);
}

TEST_F(ApexServicePrePostInstallTest, Postinstall) {
  RunPrePost(&IApexService::postinstallPackages,
             {"apex.apexd_test_postinstall.apex"},
             "sh      : PostInstall Test");
}

TEST_F(ApexServicePrePostInstallTest, MultiPostinstall) {
  constexpr const char* kLogcatText =
      "sh      : /apex/com.android.apex.test_package/etc/sample_prebuilt_file";
  RunPrePost(&IApexService::postinstallPackages,
             {"apex.apexd_test_postinstall.apex", "apex.apexd_test.apex"},
             kLogcatText);
}

TEST_F(ApexServicePrePostInstallTest, PostinstallFail) {
  RunPrePost(&IApexService::postinstallPackages,
             {"apex.apexd_test_prepostinstall.fail.apex"},
             /* test_message= */ nullptr, /* expect_success= */ false);
}

TEST_F(ApexServiceTest, SubmitSingleSessionTestSuccess) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_123",
                                      "staging_data_file");
  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 123;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)))
      << GetDebugStr(&installer);
  EXPECT_EQ(1u, list.apexInfos.size());
  ApexInfo match;
  for (const ApexInfo& info : list.apexInfos) {
    if (info.moduleName == installer.package) {
      match = info;
      break;
    }
  }

  ASSERT_EQ(installer.package, match.moduleName);
  ASSERT_EQ(installer.version, static_cast<uint64_t>(match.versionCode));
  ASSERT_EQ(installer.test_file, match.modulePath);

  ApexSessionInfo session;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(123, &session)))
      << GetDebugStr(&installer);
  ApexSessionInfo expected = CreateSessionInfo(123);
  expected.isVerified = true;
  EXPECT_THAT(session, SessionInfoEq(expected));

  ASSERT_TRUE(IsOk(service_->markStagedSessionReady(123)));
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(123, &session)))
      << GetDebugStr(&installer);
  expected.isVerified = false;
  expected.isStaged = true;
  EXPECT_THAT(session, SessionInfoEq(expected));

  // Call markStagedSessionReady again. Should be a no-op.
  ASSERT_TRUE(IsOk(service_->markStagedSessionReady(123)))
      << GetDebugStr(&installer);

  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(123, &session)))
      << GetDebugStr(&installer);
  EXPECT_THAT(session, SessionInfoEq(expected));

  // See if the session is reported with getSessions() as well
  std::vector<ApexSessionInfo> sessions;
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)))
      << GetDebugStr(&installer);
  ASSERT_THAT(sessions, UnorderedElementsAre(SessionInfoEq(expected)));
}

TEST_F(ApexServiceTest, SubmitSingleStagedSessionKeepsPreviousSessions) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_239",
                                      "staging_data_file");
  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  // First simulate existence of a bunch of sessions.
  auto session1 = ApexSession::CreateSession(37);
  ASSERT_TRUE(IsOk(session1));
  auto session2 = ApexSession::CreateSession(57);
  ASSERT_TRUE(IsOk(session2));
  auto session3 = ApexSession::CreateSession(73);
  ASSERT_TRUE(IsOk(session3));
  ASSERT_TRUE(IsOk(session1->UpdateStateAndCommit(SessionState::VERIFIED)));
  ASSERT_TRUE(IsOk(session2->UpdateStateAndCommit(SessionState::STAGED)));
  ASSERT_TRUE(IsOk(session3->UpdateStateAndCommit(SessionState::SUCCESS)));

  std::vector<ApexSessionInfo> sessions;
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));

  ApexSessionInfo expected_session1 = CreateSessionInfo(37);
  expected_session1.isVerified = true;
  ApexSessionInfo expected_session2 = CreateSessionInfo(57);
  expected_session2.isStaged = true;
  ApexSessionInfo expected_session3 = CreateSessionInfo(73);
  expected_session3.isSuccess = true;
  ASSERT_THAT(sessions, UnorderedElementsAre(SessionInfoEq(expected_session1),
                                             SessionInfoEq(expected_session2),
                                             SessionInfoEq(expected_session3)));

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 239;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  sessions.clear();
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));

  ApexSessionInfo new_session = CreateSessionInfo(239);
  new_session.isVerified = true;
  ASSERT_THAT(sessions, UnorderedElementsAre(SessionInfoEq(new_session),
                                             SessionInfoEq(expected_session1),
                                             SessionInfoEq(expected_session2),
                                             SessionInfoEq(expected_session3)));
}

TEST_F(ApexServiceTest, SubmitSingleSessionTestFail) {
  PrepareTestApexForInstall installer(
      GetTestFile("apex.apexd_test_corrupt_apex.apex"),
      "/data/app-staging/session_456", "staging_data_file");
  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 456;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)))
      << GetDebugStr(&installer);

  ApexSessionInfo session;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(456, &session)))
      << GetDebugStr(&installer);
  ApexSessionInfo expected = CreateSessionInfo(-1);
  expected.isUnknown = true;
  EXPECT_THAT(session, SessionInfoEq(expected));
}

TEST_F(ApexServiceTest, SubmitMultiSessionTestSuccess) {
  // Parent session id: 10
  // Children session ids: 20 30
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_20",
                                      "staging_data_file");
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_different_app.apex"),
      "/data/app-staging/session_30", "staging_data_file");
  if (!installer.Prepare() || !installer2.Prepare()) {
    FAIL() << GetDebugStr(&installer) << GetDebugStr(&installer2);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 10;
  params.childSessionIds = {20, 30};
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)))
      << GetDebugStr(&installer);
  EXPECT_EQ(2u, list.apexInfos.size());
  ApexInfo match;
  bool package1_found = false;
  bool package2_found = false;
  for (const ApexInfo& info : list.apexInfos) {
    if (info.moduleName == installer.package) {
      ASSERT_EQ(installer.package, info.moduleName);
      ASSERT_EQ(installer.version, static_cast<uint64_t>(info.versionCode));
      ASSERT_EQ(installer.test_file, info.modulePath);
      package1_found = true;
    } else if (info.moduleName == installer2.package) {
      ASSERT_EQ(installer2.package, info.moduleName);
      ASSERT_EQ(installer2.version, static_cast<uint64_t>(info.versionCode));
      ASSERT_EQ(installer2.test_file, info.modulePath);
      package2_found = true;
    } else {
      FAIL() << "Unexpected package found " << info.moduleName
             << GetDebugStr(&installer) << GetDebugStr(&installer2);
    }
  }
  ASSERT_TRUE(package1_found);
  ASSERT_TRUE(package2_found);

  ApexSessionInfo session;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(10, &session)))
      << GetDebugStr(&installer);
  ApexSessionInfo expected = CreateSessionInfo(10);
  expected.isVerified = true;
  ASSERT_THAT(session, SessionInfoEq(expected));

  ASSERT_TRUE(IsOk(service_->markStagedSessionReady(10)))
      << GetDebugStr(&installer);

  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(10, &session)))
      << GetDebugStr(&installer);
  expected.isVerified = false;
  expected.isStaged = true;
  ASSERT_THAT(session, SessionInfoEq(expected));
}

TEST_F(ApexServiceTest, SubmitMultiSessionTestFail) {
  // Parent session id: 11
  // Children session ids: 21 31
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"),
                                      "/data/app-staging/session_21",
                                      "staging_data_file");
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_corrupt_apex.apex"),
      "/data/app-staging/session_31", "staging_data_file");
  if (!installer.Prepare() || !installer2.Prepare()) {
    FAIL() << GetDebugStr(&installer) << GetDebugStr(&installer2);
  }
  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 11;
  params.childSessionIds = {21, 31};
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)))
      << GetDebugStr(&installer);
}

TEST_F(ApexServiceTest, MarkStagedSessionReadyFail) {
  // We should fail if we ask information about a session we don't know.
  ASSERT_FALSE(IsOk(service_->markStagedSessionReady(666)));

  ApexSessionInfo session;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(666, &session)));
  ApexSessionInfo expected = CreateSessionInfo(-1);
  expected.isUnknown = true;
  ASSERT_THAT(session, SessionInfoEq(expected));
}

TEST_F(ApexServiceTest, MarkStagedSessionSuccessfulFailsNoSession) {
  ASSERT_FALSE(IsOk(service_->markStagedSessionSuccessful(37)));

  ApexSessionInfo session_info;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(37, &session_info)));
  ApexSessionInfo expected = CreateSessionInfo(-1);
  expected.isUnknown = true;
  ASSERT_THAT(session_info, SessionInfoEq(expected));
}

TEST_F(ApexServiceTest, MarkStagedSessionSuccessfulFailsSessionInWrongState) {
  auto session = ApexSession::CreateSession(73);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(
      IsOk(session->UpdateStateAndCommit(::apex::proto::SessionState::STAGED)));

  ASSERT_FALSE(IsOk(service_->markStagedSessionSuccessful(73)));

  ApexSessionInfo session_info;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(73, &session_info)));
  ApexSessionInfo expected = CreateSessionInfo(73);
  expected.isStaged = true;
  ASSERT_THAT(session_info, SessionInfoEq(expected));
}

TEST_F(ApexServiceTest, MarkStagedSessionSuccessfulActivatedSession) {
  auto session = ApexSession::CreateSession(239);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(
      session->UpdateStateAndCommit(::apex::proto::SessionState::ACTIVATED)));

  ASSERT_TRUE(IsOk(service_->markStagedSessionSuccessful(239)));

  ApexSessionInfo session_info;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(239, &session_info)));
  ApexSessionInfo expected = CreateSessionInfo(239);
  expected.isSuccess = true;
  ASSERT_THAT(session_info, SessionInfoEq(expected));
}

TEST_F(ApexServiceTest, MarkStagedSessionSuccessfulNoOp) {
  auto session = ApexSession::CreateSession(1543);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(
      session->UpdateStateAndCommit(::apex::proto::SessionState::SUCCESS)));

  ASSERT_TRUE(IsOk(service_->markStagedSessionSuccessful(1543)));

  ApexSessionInfo session_info;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(1543, &session_info)));
  ApexSessionInfo expected = CreateSessionInfo(1543);
  expected.isSuccess = true;
  ASSERT_THAT(session_info, SessionInfoEq(expected));
}

// Should be able to abort individual staged session
TEST_F(ApexServiceTest, AbortStagedSession) {
  auto session1 = ApexSession::CreateSession(239);
  ASSERT_TRUE(IsOk(session1->UpdateStateAndCommit(SessionState::VERIFIED)));
  auto session2 = ApexSession::CreateSession(240);
  ASSERT_TRUE(IsOk(session2->UpdateStateAndCommit(SessionState::STAGED)));

  std::vector<ApexSessionInfo> sessions;
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));
  ASSERT_EQ(2u, sessions.size());

  ASSERT_TRUE(IsOk(service_->abortStagedSession(239)));

  sessions.clear();
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));
  ApexSessionInfo expected = CreateSessionInfo(240);
  expected.isStaged = true;
  ASSERT_THAT(sessions, UnorderedElementsAre(SessionInfoEq(expected)));
}

// abortStagedSession should not abort activated session
TEST_F(ApexServiceTest, AbortStagedSessionActivatedFail) {
  auto session1 = ApexSession::CreateSession(239);
  ASSERT_TRUE(IsOk(session1->UpdateStateAndCommit(SessionState::ACTIVATED)));
  auto session2 = ApexSession::CreateSession(240);
  ASSERT_TRUE(IsOk(session2->UpdateStateAndCommit(SessionState::STAGED)));

  std::vector<ApexSessionInfo> sessions;
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));
  ASSERT_EQ(2u, sessions.size());

  ASSERT_FALSE(IsOk(service_->abortStagedSession(239)));

  sessions.clear();
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));
  ApexSessionInfo expected1 = CreateSessionInfo(239);
  expected1.isActivated = true;
  ApexSessionInfo expected2 = CreateSessionInfo(240);
  expected2.isStaged = true;
  ASSERT_THAT(sessions, UnorderedElementsAre(SessionInfoEq(expected1),
                                             SessionInfoEq(expected2)));
}

TEST_F(ApexServiceTest, BackupActivePackages) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }
  PrepareTestApexForInstall installer1(GetTestFile("apex.apexd_test.apex"));
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_different_app.apex"));
  PrepareTestApexForInstall installer3(GetTestFile("apex.apexd_test_v2.apex"),
                                       "/data/app-staging/session_23",
                                       "staging_data_file");

  if (!installer1.Prepare() || !installer2.Prepare() || !installer3.Prepare()) {
    return;
  }

  // Activate some packages, in order to backup them later.
  std::vector<std::string> pkgs = {installer1.test_file, installer2.test_file};
  ASSERT_TRUE(IsOk(service_->stagePackages(pkgs)));

  // Make sure that /data/apex/active has activated packages.
  auto active_pkgs = ReadEntireDir(kActiveApexPackagesDataDir);
  ASSERT_TRUE(IsOk(active_pkgs));
  ASSERT_THAT(*active_pkgs,
              UnorderedElementsAre(installer1.test_installed_file,
                                   installer2.test_installed_file));

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 23;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  auto backups = ReadEntireDir(kApexBackupDir);
  ASSERT_TRUE(IsOk(backups));
  auto backup1 =
      StringPrintf("%s/com.android.apex.test_package@1.apex", kApexBackupDir);
  auto backup2 =
      StringPrintf("%s/com.android.apex.test_package_2@1.apex", kApexBackupDir);
  ASSERT_THAT(*backups, UnorderedElementsAre(backup1, backup2));
}

TEST_F(ApexServiceTest, BackupActivePackagesClearsPreviousBackup) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }
  PrepareTestApexForInstall installer1(GetTestFile("apex.apexd_test.apex"));
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_different_app.apex"));
  PrepareTestApexForInstall installer3(GetTestFile("apex.apexd_test_v2.apex"),
                                       "/data/app-staging/session_43",
                                       "staging_data_file");

  if (!installer1.Prepare() || !installer2.Prepare() || !installer3.Prepare()) {
    return;
  }

  // Make sure /data/apex/backups exists.
  ASSERT_TRUE(IsOk(createDirIfNeeded(std::string(kApexBackupDir), 0700)));
  // Create some bogus files in /data/apex/backups.
  std::ofstream old_backup(StringPrintf("%s/file1", kApexBackupDir));
  ASSERT_TRUE(old_backup.good());
  old_backup.close();

  std::vector<std::string> pkgs = {installer1.test_file, installer2.test_file};
  ASSERT_TRUE(IsOk(service_->stagePackages(pkgs)));

  // Make sure that /data/apex/active has activated packages.
  auto active_pkgs = ReadEntireDir(kActiveApexPackagesDataDir);
  ASSERT_TRUE(IsOk(active_pkgs));
  ASSERT_THAT(*active_pkgs,
              UnorderedElementsAre(installer1.test_installed_file,
                                   installer2.test_installed_file));

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 43;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  auto backups = ReadEntireDir(kApexBackupDir);
  ASSERT_TRUE(IsOk(backups));
  auto backup1 =
      StringPrintf("%s/com.android.apex.test_package@1.apex", kApexBackupDir);
  auto backup2 =
      StringPrintf("%s/com.android.apex.test_package_2@1.apex", kApexBackupDir);
  ASSERT_THAT(*backups, UnorderedElementsAre(backup1, backup2));
}

TEST_F(ApexServiceTest, BackupActivePackagesZeroActivePackages) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"),
                                      "/data/app-staging/session_41",
                                      "staging_data_file");

  if (!installer.Prepare()) {
    return;
  }

  // Make sure that /data/apex/active exists and is empty
  ASSERT_TRUE(
      IsOk(createDirIfNeeded(std::string(kActiveApexPackagesDataDir), 0755)));
  auto active_pkgs = ReadEntireDir(kActiveApexPackagesDataDir);
  ASSERT_TRUE(IsOk(active_pkgs));
  ASSERT_EQ(0u, active_pkgs->size());

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 41;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  auto backups = ReadEntireDir(kApexBackupDir);
  ASSERT_TRUE(IsOk(backups));
  ASSERT_EQ(0u, backups->size());
}

TEST_F(ApexServiceTest, ActivePackagesFolderDoesNotExist) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"),
                                      "/data/app-staging/session_41",
                                      "staging_data_file");

  if (!installer.Prepare()) {
    return;
  }

  // Make sure that /data/apex/active does not exist
  std::error_code ec;
  fs::remove_all(fs::path(kActiveApexPackagesDataDir), ec);
  ASSERT_FALSE(ec) << "Failed to delete " << kActiveApexPackagesDataDir;

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 41;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));

  if (!supports_fs_checkpointing_) {
    auto backups = ReadEntireDir(kApexBackupDir);
    ASSERT_TRUE(IsOk(backups));
    ASSERT_EQ(0u, backups->size());
  }
}

TEST_F(ApexServiceTest, UnstagePackagesSuccess) {
  PrepareTestApexForInstall installer1(GetTestFile("apex.apexd_test.apex"));
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_different_app.apex"));

  if (!installer1.Prepare() || !installer2.Prepare()) {
    return;
  }

  std::vector<std::string> pkgs = {installer1.test_file, installer2.test_file};
  ASSERT_TRUE(IsOk(service_->stagePackages(pkgs)));

  pkgs = {installer2.test_installed_file};
  ASSERT_TRUE(IsOk(service_->unstagePackages(pkgs)));

  auto active_packages = ReadEntireDir(kActiveApexPackagesDataDir);
  ASSERT_TRUE(IsOk(active_packages));
  ASSERT_THAT(*active_packages,
              UnorderedElementsAre(installer1.test_installed_file));
}

TEST_F(ApexServiceTest, UnstagePackagesFail) {
  PrepareTestApexForInstall installer1(GetTestFile("apex.apexd_test.apex"));
  PrepareTestApexForInstall installer2(
      GetTestFile("apex.apexd_test_different_app.apex"));

  if (!installer1.Prepare() || !installer2.Prepare()) {
    return;
  }

  std::vector<std::string> pkgs = {installer1.test_file};
  ASSERT_TRUE(IsOk(service_->stagePackages(pkgs)));

  pkgs = {installer1.test_installed_file, installer2.test_installed_file};
  ASSERT_FALSE(IsOk(service_->unstagePackages(pkgs)));

  // Check that first package wasn't unstaged.
  auto active_packages = ReadEntireDir(kActiveApexPackagesDataDir);
  ASSERT_TRUE(IsOk(active_packages));
  ASSERT_THAT(*active_packages,
              UnorderedElementsAre(installer1.test_installed_file));
}

class ApexServiceRevertTest : public ApexServiceTest {
 protected:
  void SetUp() override { ApexServiceTest::SetUp(); }

  void PrepareBackup(const std::vector<std::string>& pkgs) {
    ASSERT_TRUE(IsOk(createDirIfNeeded(std::string(kApexBackupDir), 0700)));
    for (const auto& pkg : pkgs) {
      PrepareTestApexForInstall installer(pkg);
      ASSERT_TRUE(installer.Prepare()) << " failed to prepare " << pkg;
      const std::string& from = installer.test_file;
      std::string to = std::string(kApexBackupDir) + "/" + installer.package +
                       "@" + std::to_string(installer.version) + ".apex";
      std::error_code ec;
      fs::copy(fs::path(from), fs::path(to),
               fs::copy_options::create_hard_links, ec);
      ASSERT_FALSE(ec) << "Failed to copy " << from << " to " << to << " : "
                       << ec;
    }
  }

  void CheckActiveApexContents(const std::vector<std::string>& expected_pkgs) {
    // First check that /data/apex/active exists and has correct permissions.
    struct stat sd;
    ASSERT_EQ(0, stat(kActiveApexPackagesDataDir, &sd));
    ASSERT_EQ(0755u, sd.st_mode & ALLPERMS);

    // Now read content and check it contains expected values.
    auto active_pkgs = ReadEntireDir(kActiveApexPackagesDataDir);
    ASSERT_TRUE(IsOk(active_pkgs));
    ASSERT_THAT(*active_pkgs, UnorderedElementsAreArray(expected_pkgs));
  }
};

// Should be able to revert activated sessions
TEST_F(ApexServiceRevertTest, RevertActiveSessionsSuccessful) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }

  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer.Prepare()) {
    return;
  }

  auto session = ApexSession::CreateSession(1543);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(session->UpdateStateAndCommit(SessionState::ACTIVATED)));

  // Make sure /data/apex/active is non-empty.
  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));

  PrepareBackup({GetTestFile("apex.apexd_test.apex")});

  ASSERT_TRUE(IsOk(service_->revertActiveSessions()));

  auto pkg = StringPrintf("%s/com.android.apex.test_package@1.apex",
                          kActiveApexPackagesDataDir);
  SCOPED_TRACE("");
  CheckActiveApexContents({pkg});
}

// Calling revertActiveSessions should not restore backup on checkpointing
// devices
TEST_F(ApexServiceRevertTest,
       RevertActiveSessionsDoesNotRestoreBackupIfCheckpointingSupported) {
  if (!supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is not supported";
  }

  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer.Prepare()) {
    return;
  }

  auto session = ApexSession::CreateSession(1543);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(session->UpdateStateAndCommit(SessionState::ACTIVATED)));

  // Make sure /data/apex/active is non-empty.
  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));

  PrepareBackup({GetTestFile("apex.apexd_test.apex")});

  ASSERT_TRUE(IsOk(service_->revertActiveSessions()));

  // Check that active apexes were not reverted.
  auto pkg = StringPrintf("%s/com.android.apex.test_package@2.apex",
                          kActiveApexPackagesDataDir);
  SCOPED_TRACE("");
  CheckActiveApexContents({pkg});
}

// Should fail to revert active sessions when there are none
TEST_F(ApexServiceRevertTest, RevertActiveSessionsWithoutActiveSessions) {
  // This test simulates a situation that should never happen on user builds:
  // revertActiveSessions was called, but there were no active sessions.
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer.Prepare()) {
    return;
  }

  // Make sure /data/apex/active is non-empty.
  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));

  PrepareBackup({GetTestFile("apex.apexd_test.apex")});

  // Even though backup is there, no sessions are active, hence revert request
  // should fail.
  ASSERT_FALSE(IsOk(service_->revertActiveSessions()));
}

TEST_F(ApexServiceRevertTest, RevertFailsNoBackupFolder) {
  ASSERT_FALSE(IsOk(service_->revertActiveSessions()));
}

TEST_F(ApexServiceRevertTest, RevertFailsNoActivePackagesFolder) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test.apex"));
  ASSERT_FALSE(IsOk(service_->revertActiveSessions()));
}

TEST_F(ApexServiceRevertTest, MarkStagedSessionSuccessfulCleanupBackup) {
  PrepareBackup({GetTestFile("apex.apexd_test.apex"),
                 GetTestFile("apex.apexd_test_different_app.apex")});

  auto session = ApexSession::CreateSession(101);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(session->UpdateStateAndCommit(SessionState::ACTIVATED)));

  ASSERT_TRUE(IsOk(service_->markStagedSessionSuccessful(101)));

  ASSERT_TRUE(fs::is_empty(fs::path(kApexBackupDir)));
}

TEST_F(ApexServiceRevertTest, ResumesRevert) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }
  PrepareBackup({GetTestFile("apex.apexd_test.apex"),
                 GetTestFile("apex.apexd_test_different_app.apex")});

  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer.Prepare()) {
    return;
  }

  // Make sure /data/apex/active is non-empty.
  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));

  auto session = ApexSession::CreateSession(17239);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(
      IsOk(session->UpdateStateAndCommit(SessionState::REVERT_IN_PROGRESS)));

  ASSERT_TRUE(IsOk(service_->resumeRevertIfNeeded()));

  auto pkg1 = StringPrintf("%s/com.android.apex.test_package@1.apex",
                           kActiveApexPackagesDataDir);
  auto pkg2 = StringPrintf("%s/com.android.apex.test_package_2@1.apex",
                           kActiveApexPackagesDataDir);
  SCOPED_TRACE("");
  CheckActiveApexContents({pkg1, pkg2});

  std::vector<ApexSessionInfo> sessions;
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));
  ApexSessionInfo expected = CreateSessionInfo(17239);
  expected.isReverted = true;
  ASSERT_THAT(sessions, UnorderedElementsAre(SessionInfoEq(expected)));
}

TEST_F(ApexServiceRevertTest, DoesNotResumeRevert) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer.Prepare()) {
    return;
  }

  // Make sure /data/apex/active is non-empty.
  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));

  auto session = ApexSession::CreateSession(53);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(session->UpdateStateAndCommit(SessionState::SUCCESS)));

  ASSERT_TRUE(IsOk(service_->resumeRevertIfNeeded()));

  // Check that revert wasn't resumed.
  auto active_pkgs = ReadEntireDir(kActiveApexPackagesDataDir);
  ASSERT_TRUE(IsOk(active_pkgs));
  ASSERT_THAT(*active_pkgs,
              UnorderedElementsAre(installer.test_installed_file));

  std::vector<ApexSessionInfo> sessions;
  ASSERT_TRUE(IsOk(service_->getSessions(&sessions)));
  ApexSessionInfo expected = CreateSessionInfo(53);
  expected.isSuccess = true;
  ASSERT_THAT(sessions, UnorderedElementsAre(SessionInfoEq(expected)));
}

// Should mark sessions as REVERT_FAILED on failed revert
TEST_F(ApexServiceRevertTest, SessionsMarkedAsRevertFailed) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }

  auto session = ApexSession::CreateSession(53);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(session->UpdateStateAndCommit(SessionState::ACTIVATED)));

  ASSERT_FALSE(IsOk(service_->revertActiveSessions()));
  ApexSessionInfo session_info;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(53, &session_info)));
  ApexSessionInfo expected = CreateSessionInfo(53);
  expected.isRevertFailed = true;
  ASSERT_THAT(session_info, SessionInfoEq(expected));
}

TEST_F(ApexServiceRevertTest, RevertFailedStateRevertAttemptFails) {
  if (supports_fs_checkpointing_) {
    GTEST_SKIP() << "Can't run if filesystem checkpointing is enabled";
  }

  auto session = ApexSession::CreateSession(17239);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(session->UpdateStateAndCommit(SessionState::REVERT_FAILED)));

  ASSERT_FALSE(IsOk(service_->revertActiveSessions()));
  ApexSessionInfo session_info;
  ASSERT_TRUE(IsOk(service_->getStagedSessionInfo(17239, &session_info)));
  ApexSessionInfo expected = CreateSessionInfo(17239);
  expected.isRevertFailed = true;
  ASSERT_THAT(session_info, SessionInfoEq(expected));
}

TEST_F(ApexServiceRevertTest, RevertStoresCrashingNativeProcess) {
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer.Prepare()) {
    return;
  }
  auto session = ApexSession::CreateSession(1543);
  ASSERT_TRUE(IsOk(session));
  ASSERT_TRUE(IsOk(session->UpdateStateAndCommit(SessionState::ACTIVATED)));

  // Make sure /data/apex/active is non-empty.
  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
  std::string native_process = "test_process";
  Result<void> res = ::android::apex::revertActiveSessions(native_process);
  session = ApexSession::GetSession(1543);
  ASSERT_EQ(session->GetCrashingNativeProcess(), native_process);
}

static pid_t GetPidOf(const std::string& name) {
  char buf[1024];
  const std::string cmd = std::string("pidof -s ") + name;
  FILE* cmd_pipe = popen(cmd.c_str(), "r");
  if (cmd_pipe == nullptr) {
    PLOG(ERROR) << "Cannot open pipe for " << cmd;
    return 0;
  }
  if (fgets(buf, 1024, cmd_pipe) == nullptr) {
    PLOG(ERROR) << "Cannot read pipe for " << cmd;
    pclose(cmd_pipe);
    return 0;
  }

  pclose(cmd_pipe);
  return strtoul(buf, nullptr, 10);
}

static void ExecInMountNamespaceOf(pid_t pid,
                                   const std::function<void(pid_t)>& func) {
  const std::string my_path = "/proc/self/ns/mnt";
  android::base::unique_fd my_fd(open(my_path.c_str(), O_RDONLY | O_CLOEXEC));
  ASSERT_TRUE(my_fd.get() >= 0);

  const std::string target_path =
      std::string("/proc/") + std::to_string(pid) + "/ns/mnt";
  android::base::unique_fd target_fd(
      open(target_path.c_str(), O_RDONLY | O_CLOEXEC));
  ASSERT_TRUE(target_fd.get() >= 0);

  int res = setns(target_fd.get(), CLONE_NEWNS);
  ASSERT_NE(-1, res);

  func(pid);

  res = setns(my_fd.get(), CLONE_NEWNS);
  ASSERT_NE(-1, res);
}

TEST(ApexdTest, ApexdIsInSameMountNamespaceAsInit) {
  // TODO(b/136647373): Move this check to environment setup
  if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
    GTEST_SKIP() << "Skipping test because device doesn't support APEX";
  }
  std::string ns_apexd;
  std::string ns_init;

  ExecInMountNamespaceOf(GetPidOf("apexd"), [&](pid_t /*pid*/) {
    bool res = android::base::Readlink("/proc/self/ns/mnt", &ns_apexd);
    ASSERT_TRUE(res);
  });

  ExecInMountNamespaceOf(1, [&](pid_t /*pid*/) {
    bool res = android::base::Readlink("/proc/self/ns/mnt", &ns_init);
    ASSERT_TRUE(res);
  });

  ASSERT_EQ(ns_apexd, ns_init);
}

// These are NOT exhaustive list of early processes be should be enough
static const std::vector<const std::string> kEarlyProcesses = {
    "servicemanager",
    "hwservicemanager",
    "vold",
    "logd",
};

TEST(ApexdTest, EarlyProcessesAreInDifferentMountNamespace) {
  // TODO(b/136647373): Move this check to environment setup
  if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
    GTEST_SKIP() << "Skipping test because device doesn't support APEX";
  }
  std::string ns_apexd;

  ExecInMountNamespaceOf(GetPidOf("apexd"), [&](pid_t /*pid*/) {
    bool res = android::base::Readlink("/proc/self/ns/mnt", &ns_apexd);
    ASSERT_TRUE(res);
  });

  for (const auto& name : kEarlyProcesses) {
    std::string ns_early_process;
    ExecInMountNamespaceOf(GetPidOf(name), [&](pid_t /*pid*/) {
      bool res =
          android::base::Readlink("/proc/self/ns/mnt", &ns_early_process);
      ASSERT_TRUE(res);
    });
    ASSERT_NE(ns_apexd, ns_early_process);
  }
}

TEST(ApexdTest, ApexIsAPrivateMountPoint) {
  // TODO(b/136647373): Move this check to environment setup
  if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
    GTEST_SKIP() << "Skipping test because device doesn't support APEX";
  }
  std::string mountinfo;
  ASSERT_TRUE(
      android::base::ReadFileToString("/proc/self/mountinfo", &mountinfo));
  bool found_apex_mountpoint = false;
  for (const auto& line : android::base::Split(mountinfo, "\n")) {
    std::vector<std::string> tokens = android::base::Split(line, " ");
    // line format:
    // mnt_id parent_mnt_id major:minor source target option propagation_type
    // ex) 33 260:19 / /apex rw,nosuid,nodev -
    if (tokens.size() >= 7 && tokens[4] == "/apex") {
      found_apex_mountpoint = true;
      // Make sure that propagation type is set to - which means private
      ASSERT_EQ("-", tokens[6]);
    }
  }
  ASSERT_TRUE(found_apex_mountpoint);
}

static const std::vector<const std::string> kEarlyApexes = {
    "/apex/com.android.runtime",
    "/apex/com.android.tzdata",
};

TEST(ApexdTest, ApexesAreActivatedForEarlyProcesses) {
  // TODO(b/136647373): Move this check to environment setup
  if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
    GTEST_SKIP() << "Skipping test because device doesn't support APEX";
  }
  for (const auto& name : kEarlyProcesses) {
    pid_t pid = GetPidOf(name);
    const std::string path =
        std::string("/proc/") + std::to_string(pid) + "/mountinfo";
    std::string mountinfo;
    ASSERT_TRUE(android::base::ReadFileToString(path.c_str(), &mountinfo));

    std::unordered_set<std::string> mountpoints;
    for (const auto& line : android::base::Split(mountinfo, "\n")) {
      std::vector<std::string> tokens = android::base::Split(line, " ");
      // line format:
      // mnt_id parent_mnt_id major:minor source target option propagation_type
      // ex) 69 33 7:40 / /apex/com.android.conscrypt ro,nodev,noatime -
      if (tokens.size() >= 5) {
        // token[4] is the target mount point
        mountpoints.emplace(tokens[4]);
      }
    }
    for (const auto& apex_name : kEarlyApexes) {
      ASSERT_NE(mountpoints.end(), mountpoints.find(apex_name));
    }
  }
}

class ApexShimUpdateTest : public ApexServiceTest {
 protected:
  void SetUp() override {
    // TODO(b/136647373): Move this check to environment setup
    if (!android::base::GetBoolProperty("ro.apex.updatable", false)) {
      GTEST_SKIP() << "Skipping test because device doesn't support APEX";
    }
    ApexServiceTest::SetUp();

    // Assert that shim apex is pre-installed.
    std::vector<ApexInfo> list;
    ASSERT_TRUE(IsOk(service_->getAllPackages(&list)));
    ApexInfo expected;
    expected.moduleName = "com.android.apex.cts.shim";
    expected.modulePath = "/system/apex/com.android.apex.cts.shim.apex";
    expected.preinstalledModulePath =
        "/system/apex/com.android.apex.cts.shim.apex";
    expected.versionCode = 1;
    expected.isFactory = true;
    expected.isActive = true;
    ASSERT_THAT(list, Contains(ApexInfoEq(expected)));
  }
};

TEST_F(ApexShimUpdateTest, UpdateToV2Success) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.v2.apex"));

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
}

TEST_F(ApexShimUpdateTest, UpdateToV2FailureWrongSHA512) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.v2_wrong_sha.apex"));

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  const auto& status = service_->stagePackages({installer.test_file});
  ASSERT_FALSE(IsOk(status));
  const std::string& error_message =
      std::string(status.exceptionMessage().c_str());
  ASSERT_THAT(error_message, HasSubstr("has unexpected SHA512 hash"));
}

TEST_F(ApexShimUpdateTest, SubmitStagedSessionFailureHasPreInstallHook) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.v2_with_pre_install_hook.apex"),
      "/data/app-staging/session_23", "staging_data_file");

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 23;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexShimUpdateTest, SubmitStagedSessionFailureHasPostInstallHook) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.v2_with_post_install_hook.apex"),
      "/data/app-staging/session_43", "staging_data_file");

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 43;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexShimUpdateTest, SubmitStagedSessionFailureAdditionalFile) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.v2_additional_file.apex"),
      "/data/app-staging/session_41", "staging_data_file");
  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 41;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexShimUpdateTest, SubmitStagedSessionFailureAdditionalFolder) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.v2_additional_folder.apex"),
      "/data/app-staging/session_42", "staging_data_file");
  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 42;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexShimUpdateTest, UpdateToV1Success) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.apex"));

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ASSERT_TRUE(IsOk(service_->stagePackages({installer.test_file})));
}

TEST_F(ApexShimUpdateTest, SubmitStagedSessionV1ShimApexSuccess) {
  PrepareTestApexForInstall installer(
      GetTestFile("com.android.apex.cts.shim.apex"),
      "/data/app-staging/session_97", "staging_data_file");
  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 97;
  ASSERT_TRUE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexServiceTest, SubmitStagedSessionCorruptApexFails) {
  PrepareTestApexForInstall installer(
      GetTestFile("apex.apexd_test_corrupt_apex.apex"),
      "/data/app-staging/session_57", "staging_data_file");

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 57;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexServiceTest, SubmitStagedSessionCorruptApexFails_b146895998) {
  PrepareTestApexForInstall installer(GetTestFile("corrupted_b146895998.apex"),
                                      "/data/app-staging/session_71",
                                      "staging_data_file");

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ApexInfoList list;
  ApexSessionParams params;
  params.sessionId = 71;
  ASSERT_FALSE(IsOk(service_->submitStagedSession(params, &list)));
}

TEST_F(ApexServiceTest, StageCorruptApexFails_b146895998) {
  PrepareTestApexForInstall installer(GetTestFile("corrupted_b146895998.apex"));

  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }

  ASSERT_FALSE(IsOk(service_->stagePackages({installer.test_file})));
}

TEST_F(ApexServiceTest, RemountPackagesPackageOnSystemChanged) {
  static constexpr const char* kSystemPath =
      "/system_ext/apex/apex.apexd_test.apex";
  static constexpr const char* kPackageName = "com.android.apex.test_package";
  if (!fs_mgr_overlayfs_is_setup()) {
    GTEST_SKIP() << "/system_ext is not overlayed into read-write";
  }
  if (auto res = IsActive(kPackageName); !res.ok()) {
    FAIL() << res.error();
  } else {
    ASSERT_FALSE(*res) << kPackageName << " is active";
  }
  ASSERT_EQ(0, access(kSystemPath, F_OK))
      << "Failed to stat " << kSystemPath << " : " << strerror(errno);
  ASSERT_TRUE(IsOk(service_->activatePackage(kSystemPath)));
  std::string backup_path = GetTestFile("apex.apexd_test.apexd.bak");
  // Copy original /system_ext apex file. We will need to restore it after test
  // runs.
  ASSERT_RESULT_OK(CopyFile(kSystemPath, backup_path, fs::copy_options::none));

  // Make sure we cleanup after ourselves.
  auto deleter = android::base::make_scope_guard([&]() {
    if (auto ret = service_->deactivatePackage(kSystemPath); !ret.isOk()) {
      LOG(ERROR) << ret.exceptionMessage();
    }
    auto ret = CopyFile(backup_path, kSystemPath,
                        fs::copy_options::overwrite_existing);
    if (!ret.ok()) {
      LOG(ERROR) << ret.error();
    }
  });

  // Copy v2 version to /system_ext/apex/ and then call remountPackages.
  PrepareTestApexForInstall installer(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer.Prepare()) {
    FAIL() << GetDebugStr(&installer);
  }
  ASSERT_RESULT_OK(CopyFile(installer.test_file, kSystemPath,
                            fs::copy_options::overwrite_existing));
  // Don't check that remountPackages succeeded. Most likely it will fail, but
  // it should still remount our test apex.
  service_->remountPackages();

  // Check that v2 is now active.
  auto active_apex = GetActivePackage("com.android.apex.test_package");
  ASSERT_RESULT_OK(active_apex);
  ASSERT_EQ(2u, active_apex->versionCode);
  // Sanity check that module path didn't change.
  ASSERT_EQ(kSystemPath, active_apex->modulePath);
}

TEST_F(ApexServiceActivationSuccessTest, RemountPackagesPackageOnDataChanged) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());
  // Copy v2 version to /data/apex/active and then call remountPackages.
  PrepareTestApexForInstall installer2(GetTestFile("apex.apexd_test_v2.apex"));
  if (!installer2.Prepare()) {
    FAIL() << GetDebugStr(&installer2);
  }
  ASSERT_RESULT_OK(CopyFile(installer2.test_file,
                            installer_->test_installed_file,
                            fs::copy_options::overwrite_existing));
  // Don't check that remountPackages succeeded. Most likely it will fail, but
  // it should still remount our test apex.
  service_->remountPackages();

  // Check that v2 is now active.
  auto active_apex = GetActivePackage("com.android.apex.test_package");
  ASSERT_RESULT_OK(active_apex);
  ASSERT_EQ(2u, active_apex->versionCode);
  // Sanity check that module path didn't change.
  ASSERT_EQ(installer_->test_installed_file, active_apex->modulePath);
}

class LogTestToLogcat : public ::testing::EmptyTestEventListener {
  void OnTestStart(const ::testing::TestInfo& test_info) override {
#ifdef __ANDROID__
    using base::LogId;
    using base::LogSeverity;
    using base::StringPrintf;
    base::LogdLogger l;
    std::string msg =
        StringPrintf("=== %s::%s (%s:%d)", test_info.test_case_name(),
                     test_info.name(), test_info.file(), test_info.line());
    l(LogId::MAIN, LogSeverity::INFO, "ApexTestCases", __FILE__, __LINE__,
      msg.c_str());
#else
    UNUSED(test_info);
#endif
  }
};

struct NoCodeApexNameProvider {
  static std::string GetTestName() { return "apex.apexd_test_nocode.apex"; }
  static std::string GetPackageName() {
    return "com.android.apex.test_package";
  }
};

class ApexServiceActivationNoCode
    : public ApexServiceActivationTest<NoCodeApexNameProvider> {};

TEST_F(ApexServiceActivationNoCode, NoCodeApexIsNotExecutable) {
  ASSERT_TRUE(IsOk(service_->activatePackage(installer_->test_installed_file)))
      << GetDebugStr(installer_.get());

  std::string mountinfo;
  ASSERT_TRUE(
      android::base::ReadFileToString("/proc/self/mountinfo", &mountinfo));
  bool found_apex_mountpoint = false;
  for (const auto& line : android::base::Split(mountinfo, "\n")) {
    std::vector<std::string> tokens = android::base::Split(line, " ");
    // line format:
    // mnt_id parent_mnt_id major:minor source target option propagation_type
    // ex) 33 260:19 / /apex rw,nosuid,nodev -
    if (tokens.size() >= 7 &&
        tokens[4] ==
            "/apex/" + NoCodeApexNameProvider::GetPackageName() + "@1") {
      found_apex_mountpoint = true;
      // Make sure that option contains noexec
      std::vector<std::string> options = android::base::Split(tokens[5], ",");
      EXPECT_NE(options.end(),
                std::find(options.begin(), options.end(), "noexec"));
      break;
    }
  }
  EXPECT_TRUE(found_apex_mountpoint);
}

}  // namespace apex
}  // namespace android

int main(int argc, char** argv) {
  android::base::InitLogging(argv, &android::base::StderrLogger);
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::UnitTest::GetInstance()->listeners().Append(
      new android::apex::LogTestToLogcat());
  return RUN_ALL_TESTS();
}
