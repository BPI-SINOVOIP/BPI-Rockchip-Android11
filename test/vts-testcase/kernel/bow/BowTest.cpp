/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless requied by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#define LOG_TAG "BowTest"

#include <fstream>
#include <string>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>
#include <libdm/dm.h>
#include <utils/Log.h>

namespace android {

using base::unique_fd;
using namespace dm;

bool blockCheckpointsSupported() {
  static bool supported = false;
  static bool evaluated = false;

  if (evaluated) return supported;

  pid_t pid = fork();
  EXPECT_NE(pid, -1);

  if (pid == 0) {
    static const char* args[] = {"/system/bin/vdc", "checkpoint",
                                 "supportsBlockCheckpoint", 0};
    EXPECT_NE(execv(args[0], const_cast<char* const*>(args)), -1);
  }

  int status;
  EXPECT_NE(waitpid(pid, &status, 0), -1);

  supported = status == 1;
  evaluated = true;
  return supported;
}

template <void (*Prepare)(std::string)>
class LoopbackTestFixture : public ::testing::Test {
 protected:
  void SetUp() {
    Prepare(loop_file_);

    // Get free loop device name
    unique_fd cfd(open("/dev/loop-control", O_RDWR));
    ASSERT_NE(cfd.get(), -1);
    int i = ioctl(cfd, 0x4C82);  // LOOP_CTL_GET_FREE
    ASSERT_GE(i, 0);
    loop_device_ = std::string("/dev/block/loop") + std::to_string(i);

    // Associate loop device with file
    unique_fd lfd(open(loop_device_.c_str(), O_RDWR));
    ASSERT_NE(lfd.get(), -1);
    unique_fd ffd(open(loop_file_.c_str(), O_RDWR));
    ASSERT_NE(ffd.get(), -1);
    ASSERT_EQ(ioctl(lfd.get(), LOOP_SET_FD, ffd.get()), 0);
  }

  void TearDown() {
    unique_fd lfd(open(loop_device_.c_str(), O_RDWR));
    EXPECT_NE(lfd.get(), -1);
    EXPECT_EQ(ioctl(lfd, LOOP_CLR_FD, 0), 0);
    EXPECT_EQ(remove(loop_file_.c_str()), 0);
  }

  const static std::string loop_file_;

 public:
  const static size_t sector_size_ = 512;
  const static size_t loop_size_ = 4096 * sector_size_;
  std::string loop_device_;
};

template <void (*Prepare)(std::string)>
const std::string LoopbackTestFixture<Prepare>::loop_file_ =
    "/data/local/tmp/bow_loop";

void PrepareBowDefault(std::string) {}

template <void (*PrepareLoop)(std::string),
          void (*PrepareBow)(std::string) = PrepareBowDefault>
class BowTestFixture : public LoopbackTestFixture<PrepareLoop> {
  std::string GetTableStatus() {
    std::vector<DeviceMapper::TargetInfo> targets;
    DeviceMapper& dm = DeviceMapper::Instance();
    EXPECT_TRUE(dm.GetTableInfo("bow1", &targets));
    EXPECT_EQ(targets.size(), 1);
    return targets[0].data;
  }

  bool torn_down_;

 protected:
  void SetUp() {
    if (!blockCheckpointsSupported()) return;

    LoopbackTestFixture<PrepareLoop>::SetUp();
    PrepareBow(loop_device_);

    torn_down_ = false;

    DmTable table;
    table.AddTarget(std::make_unique<DmTargetBow>(0, loop_size_ / 512,
                                                  loop_device_.c_str()));

    DeviceMapper& dm = DeviceMapper::Instance();
    ASSERT_TRUE(dm.CreateDevice("bow1", table));
    ASSERT_TRUE(dm.GetDmDevicePathByName("bow1", &bow_device_));
  }

  void TearDown() {
    if (!blockCheckpointsSupported()) return;
    BowTearDown();
    LoopbackTestFixture<PrepareLoop>::TearDown();
  }

  bool TornDown() const { return torn_down_; }

 public:
  using LoopbackTestFixture<PrepareLoop>::loop_size_;
  using LoopbackTestFixture<PrepareLoop>::sector_size_;
  using LoopbackTestFixture<PrepareLoop>::loop_device_;

  void BowTearDown() {
    if (torn_down_) return;
    torn_down_ = true;
    EXPECT_TRUE(DeviceMapper::Instance().DeleteDevice("bow1"));
  }

  void SetState(int i) {
    std::string state_file = "/sys" + bow_device_.substr(4) + "/bow/state";
    std::ofstream(state_file) << i;

    int j;
    std::ifstream(state_file) >> j;
    EXPECT_EQ(i, j);
  }

  enum SectorTypes {
    INVALID,
    SECTOR0,
    SECTOR0_CURRENT,
    UNCHANGED,
    BACKUP,
    FREE,
    CHANGED,
    TOP
  };

  struct TableEntry {
    SectorTypes type;
    uint64_t offset;

    bool operator==(const TableEntry& te) const {
      return type == te.type && offset == te.offset;
    }
  };

  std::vector<TableEntry> GetTable() {
    std::string status = GetTableStatus();
    std::istringstream i(status);
    std::vector<TableEntry> table;
    while (true) {
      TableEntry te = {};
      std::string s;
      i >> s >> te.offset;
      if (!i) {
        EXPECT_EQ(s, "");
        break;
      }

      if (s == "Sector0:")
        te.type = SECTOR0;
      else if (s == "Sector0_current:")
        te.type = SECTOR0_CURRENT;
      else if (s == "Unchanged:")
        te.type = UNCHANGED;
      else if (s == "Backup:")
        te.type = BACKUP;
      else if (s == "Free:")
        te.type = FREE;
      else if (s == "Changed:")
        te.type = CHANGED;
      else if (s == "Top:")
        te.type = TOP;
      else
        ADD_FAILURE();

      te.offset /= sector_size_ / 512;

      table.push_back(te);
    }
    return table;
  }

  std::string bow_device_;
};

void PrepareFile(std::string loop_file) {
  auto const& sector_size_ = LoopbackTestFixture<PrepareFile>::sector_size_;
  auto const& loop_size_ = LoopbackTestFixture<PrepareFile>::loop_size_;

  unique_fd fd(
      open(loop_file.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR));
  ASSERT_NE(fd.get(), -1);
  for (int i = 0; i < loop_size_ / sector_size_; ++i) {
    char buffer[sector_size_] = {};
    snprintf(buffer, sizeof(buffer), "Sector %d", i);
    write(fd.get(), buffer, sizeof(buffer));
  }
}

class FileBowTestFixture : public BowTestFixture<&PrepareFile> {
  void SetUp() {
    if (!blockCheckpointsSupported()) return;
    BowTestFixture<&PrepareFile>::SetUp();
    fd_ = unique_fd(open(bow_device_.c_str(), O_RDWR));
    ASSERT_NE(fd_.get(), -1);
  }

  void TearDown() {
    fd_ = unique_fd();
    BowTestFixture<&PrepareFile>::TearDown();
  }

  unique_fd fd_;

 public:
  void Discard(uint64_t offset, uint64_t length) {
    uint64_t range[2] = {offset * sector_size_, length * sector_size_};
    EXPECT_EQ(ioctl(fd_.get(), BLKDISCARD, range), 0);
  }

  int Write(SectorTypes type) {
    for (auto i : GetTable())
      if (i.type == type) {
        EXPECT_NE(lseek(fd_, i.offset * sector_size_, SEEK_SET), -1);
        EXPECT_EQ(write(fd_, "Changed", 8), 8);
        return i.offset;
      }
    EXPECT_TRUE(false);
    return -1;
  }

  void FindChanged(std::vector<TableEntry> const& free, int expected_changed) {
    unique_fd fd;
    if (TornDown())
      fd = unique_fd(open(loop_device_.c_str(), O_RDONLY));
    else
      fd = unique_fd(open(bow_device_.c_str(), O_RDONLY));
    EXPECT_NE(fd.get(), -1);
    if (fd.get() == -1) return;

    int changed = -1;
    for (int i = 0; i < loop_size_ / sector_size_; ++i) {
      if (i != expected_changed) {
        auto type = SECTOR0;
        for (auto j : free)
          if (j.offset > i)
            break;
          else
            type = j.type;
        if (type == FREE) continue;
      }

      char buffer[sector_size_];
      EXPECT_NE(lseek(fd.get(), i * sector_size_, SEEK_SET), -1);
      EXPECT_EQ(read(fd.get(), buffer, sizeof(buffer)), sizeof(buffer));
      if (strcmp(buffer, "Changed") == 0) {
        EXPECT_EQ(changed, -1);
        changed = i;
      } else {
        std::string expected = "Sector " + std::to_string(i);
        EXPECT_STREQ(buffer, expected.c_str());
      }
    }
    EXPECT_EQ(changed, expected_changed);
  }

  void DumpSector0() {
    char buffer[sector_size_];
    lseek(fd_.get(), 0, SEEK_SET);
    read(fd_.get(), buffer, sizeof(buffer));
    for (int i = 0; i < sector_size_ * 2; ++i) {
      std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (int)buffer[i];
      if (i % 32 == 31)
        std::cout << std::endl;
      else
        std::cout << " ";
    }
  }

  void WriteSubmit(SectorTypes type) {
    if (!blockCheckpointsSupported()) return;
    Discard(1, 1);
    Discard(3, 2);
    auto free = GetTable();

    SetState(1);
    auto changed = Write(type);

    SetState(2);
    FindChanged(free, changed);
  }

  void WriteRestore(SectorTypes type) {
    if (!blockCheckpointsSupported()) return;
    Discard(1, 1);
    Discard(3, 2);
    auto free = GetTable();
    SetState(1);
    Write(type);
    BowTearDown();
    system(std::string("vdc checkpoint restoreCheckpoint " + loop_device_)
               .c_str());
    FindChanged(free, -1);
  }
};

TEST_F(FileBowTestFixture, discardVisible) {
  if (!blockCheckpointsSupported()) return;
  Discard(8, 1);
  Discard(16, 1);
  Discard(12, 1);
  Discard(4, 1);

  std::vector<TableEntry> table = {
      {UNCHANGED, 0},  {FREE, 4},
      {UNCHANGED, 5},  {FREE, 8},
      {UNCHANGED, 9},  {FREE, 12},
      {UNCHANGED, 13}, {FREE, 16},
      {UNCHANGED, 17}, {TOP, loop_size_ / sector_size_},
  };

  EXPECT_EQ(GetTable(), table);
}

TEST_F(FileBowTestFixture, writeSector0Submit) {
  SCOPED_TRACE("write submit SECTOR0");
  WriteSubmit(SECTOR0);
}

TEST_F(FileBowTestFixture, writeSector0Revert) {
  SCOPED_TRACE("write restore SECTOR0");
  WriteRestore(SECTOR0);
}

TEST_F(FileBowTestFixture, writeSector0_CurrentSubmit) {
  SCOPED_TRACE("write submit SECTOR0_CURRENT");
  WriteSubmit(SECTOR0_CURRENT);
}

TEST_F(FileBowTestFixture, writeSector0_CurrentRevert) {
  SCOPED_TRACE("write restore SECTOR0_CURRENT");
  WriteRestore(SECTOR0_CURRENT);
}

TEST_F(FileBowTestFixture, writeUnchangedSubmit) {
  SCOPED_TRACE("write submit UNCHANGED");
  WriteSubmit(UNCHANGED);
}

TEST_F(FileBowTestFixture, writeUnchangedRevert) {
  SCOPED_TRACE("write restore UNCHANGED");
  WriteRestore(UNCHANGED);
}

TEST_F(FileBowTestFixture, writeBackupSubmit) {
  SCOPED_TRACE("write submit BACKUP");
  WriteSubmit(BACKUP);
}

TEST_F(FileBowTestFixture, writeBackupRevert) {
  SCOPED_TRACE("write restore BACKUP");
  WriteRestore(BACKUP);
}

TEST_F(FileBowTestFixture, writeFreeSubmit) {
  SCOPED_TRACE("write submit FREE");
  WriteSubmit(FREE);
}

TEST_F(FileBowTestFixture, writeFreeRevert) {
  SCOPED_TRACE("write restore FREE");
  WriteRestore(FREE);
}

/* There are no changed sectors at start, so these can't work as is
TEST_F(BowTestFixture, writeChangedSubmit) {
    SCOPED_TRACE("write submit CHANGED");
    WriteSubmit(CHANGED);
}

TEST_F(BowTestFixture, writeChangedRevert) {
    SCOPED_TRACE("write restore CHANGED");
    WriteRestore(CHANGED);
}
*/

void PrepareFileSystem(std::string loop_file) {
  EXPECT_EQ(
      system((std::string("dd if=/dev/zero bs=512 count=4096 of=") + loop_file)
                 .c_str()),
      0);
}

#define MOUNT_POINT "/data/local/tmp/mount"

void SetupFileSystem(std::string loop_device) {
  EXPECT_EQ(system((std::string("mke2fs ") + loop_device).c_str()), 0);
  EXPECT_EQ(system("mkdir " MOUNT_POINT), 0);
  EXPECT_EQ(
      system((std::string("mount ") + loop_device + " " MOUNT_POINT).c_str()),
      0);
  EXPECT_EQ(system("echo Original > " MOUNT_POINT "/file"), 0);
  EXPECT_EQ(system("umount -D " MOUNT_POINT), 0);
  EXPECT_EQ(system("rmdir " MOUNT_POINT), 0);
}

void Trim() {
  unique_fd fd(open(MOUNT_POINT, O_RDONLY));
  EXPECT_NE(fd.get(), -1);
  struct fstrim_range range = {};
  range.len = ULLONG_MAX;
  EXPECT_EQ(ioctl(fd, FITRIM, &range), 0);
}

typedef BowTestFixture<&PrepareFileSystem, &SetupFileSystem>
    FileSystemBowTestFixture;

TEST_F(FileSystemBowTestFixture, filesystemSubmit) {
  if (!blockCheckpointsSupported()) return;
  EXPECT_EQ(system("mkdir " MOUNT_POINT), 0);
  EXPECT_EQ(
      system((std::string("mount ") + bow_device_ + " " MOUNT_POINT).c_str()),
      0);
  Trim();
  SetState(1);
  EXPECT_EQ(system("echo Changed > " MOUNT_POINT "/file"), 0);
  SetState(2);
  EXPECT_EQ(system("umount -D " MOUNT_POINT), 0);
  BowTearDown();
  EXPECT_EQ(
      system((std::string("mount ") + loop_device_ + " " MOUNT_POINT).c_str()),
      0);
  std::string contents;
  std::ifstream(MOUNT_POINT "/file") >> contents;
  EXPECT_EQ(contents, std::string("Changed"));
  EXPECT_EQ(system("umount -D " MOUNT_POINT), 0);
  EXPECT_EQ(system("rmdir " MOUNT_POINT), 0);
}

TEST_F(FileSystemBowTestFixture, filesystemRevert) {
  if (!blockCheckpointsSupported()) return;
  EXPECT_EQ(system("mkdir " MOUNT_POINT), 0);
  EXPECT_EQ(
      system((std::string("mount ") + bow_device_ + " " MOUNT_POINT).c_str()),
      0);
  Trim();
  SetState(1);
  EXPECT_EQ(system("echo Changed > " MOUNT_POINT "/file"), 0);
  EXPECT_EQ(system("umount -D " MOUNT_POINT), 0);
  BowTearDown();
  system((std::string("vdc checkpoint restoreCheckpoint ") + loop_device_)
             .c_str());
  EXPECT_EQ(
      system((std::string("mount ") + loop_device_ + " " MOUNT_POINT).c_str()),
      0);
  std::string contents;
  std::ifstream(MOUNT_POINT "/file") >> contents;
  EXPECT_EQ(contents, std::string("Original"));
  EXPECT_EQ(system("umount -D " MOUNT_POINT), 0);
  EXPECT_EQ(system("rmdir " MOUNT_POINT), 0);
}

}  // namespace android
