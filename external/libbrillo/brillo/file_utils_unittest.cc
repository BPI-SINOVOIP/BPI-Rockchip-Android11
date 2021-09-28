// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brillo/file_utils.h"

#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

namespace brillo {

namespace {

constexpr int kPermissions600 =
    base::FILE_PERMISSION_READ_BY_USER | base::FILE_PERMISSION_WRITE_BY_USER;
constexpr int kPermissions700 = base::FILE_PERMISSION_USER_MASK;
constexpr int kPermissions777 = base::FILE_PERMISSION_MASK;

std::string GetRandomSuffix() {
  const int kBufferSize = 6;
  unsigned char buffer[kBufferSize];
  base::RandBytes(buffer, arraysize(buffer));
  return base::HexEncode(buffer, arraysize(buffer));
}

}  // namespace

class FileUtilsTest : public testing::Test {
 public:
  FileUtilsTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append("test.temp");
  }

 protected:
  base::FilePath file_path_;
  base::ScopedTempDir temp_dir_;

  // Writes |contents| to |file_path_|. Pulled into a separate function just
  // to improve readability of tests.
  void WriteFile(const std::string& contents) {
    EXPECT_EQ(contents.length(),
              base::WriteFile(file_path_, contents.c_str(), contents.length()));
  }

  // Verifies that the file at |file_path_| exists and contains |contents|.
  void ExpectFileContains(const std::string& contents) {
    EXPECT_TRUE(base::PathExists(file_path_));
    std::string new_contents;
    EXPECT_TRUE(base::ReadFileToString(file_path_, &new_contents));
    EXPECT_EQ(contents, new_contents);
  }

  // Verifies that the file at |file_path_| has |permissions|.
  void ExpectFilePermissions(int permissions) {
    int actual_permissions;
    EXPECT_TRUE(base::GetPosixFilePermissions(file_path_, &actual_permissions));
    EXPECT_EQ(permissions, actual_permissions);
  }

  // Creates a file with a random name in the temporary directory.
  base::FilePath GetTempName() {
    return temp_dir_.GetPath().Append(GetRandomSuffix());
  }
};

TEST_F(FileUtilsTest, TouchFileCreate) {
  EXPECT_TRUE(TouchFile(file_path_));
  ExpectFileContains("");
  ExpectFilePermissions(kPermissions600);
}

TEST_F(FileUtilsTest, TouchFileCreateThroughUmask) {
  mode_t old_umask = umask(kPermissions777);
  EXPECT_TRUE(TouchFile(file_path_));
  umask(old_umask);
  ExpectFileContains("");
  ExpectFilePermissions(kPermissions600);
}

TEST_F(FileUtilsTest, TouchFileCreateDirectoryStructure) {
  file_path_ = temp_dir_.GetPath().Append("foo/bar/baz/test.temp");
  EXPECT_TRUE(TouchFile(file_path_));
  ExpectFileContains("");
}

TEST_F(FileUtilsTest, TouchFileExisting) {
  WriteFile("abcd");
  EXPECT_TRUE(TouchFile(file_path_));
  ExpectFileContains("abcd");
}

TEST_F(FileUtilsTest, TouchFileReplaceDirectory) {
  EXPECT_TRUE(base::CreateDirectory(file_path_));
  EXPECT_TRUE(TouchFile(file_path_));
  EXPECT_FALSE(base::DirectoryExists(file_path_));
  ExpectFileContains("");
}

TEST_F(FileUtilsTest, TouchFileReplaceSymlink) {
  base::FilePath symlink_target = temp_dir_.GetPath().Append("target.temp");
  EXPECT_TRUE(base::CreateSymbolicLink(symlink_target, file_path_));
  EXPECT_TRUE(TouchFile(file_path_));
  EXPECT_FALSE(base::IsLink(file_path_));
  ExpectFileContains("");
}

TEST_F(FileUtilsTest, TouchFileReplaceOtherUser) {
  WriteFile("abcd");
  EXPECT_TRUE(TouchFile(file_path_, kPermissions777, geteuid() + 1, getegid()));
  ExpectFileContains("");
}

TEST_F(FileUtilsTest, TouchFileReplaceOtherGroup) {
  WriteFile("abcd");
  EXPECT_TRUE(TouchFile(file_path_, kPermissions777, geteuid(), getegid() + 1));
  ExpectFileContains("");
}

TEST_F(FileUtilsTest, TouchFileCreateWithAllPermissions) {
  EXPECT_TRUE(TouchFile(file_path_, kPermissions777, geteuid(), getegid()));
  ExpectFileContains("");
  ExpectFilePermissions(kPermissions777);
}

TEST_F(FileUtilsTest, TouchFileCreateWithOwnerPermissions) {
  EXPECT_TRUE(TouchFile(file_path_, kPermissions700, geteuid(), getegid()));
  ExpectFileContains("");
  ExpectFilePermissions(kPermissions700);
}

TEST_F(FileUtilsTest, TouchFileExistingPermissionsUnchanged) {
  EXPECT_TRUE(TouchFile(file_path_, kPermissions777, geteuid(), getegid()));
  EXPECT_TRUE(TouchFile(file_path_, kPermissions700, geteuid(), getegid()));
  ExpectFileContains("");
  ExpectFilePermissions(kPermissions777);
}

TEST_F(FileUtilsTest, WriteFileCanBeReadBack) {
  const base::FilePath filename(GetTempName());
  const std::string content("blablabla");
  EXPECT_TRUE(WriteStringToFile(filename, content));
  std::string output;
  EXPECT_TRUE(ReadFileToString(filename, &output));
  EXPECT_EQ(content, output);
}

TEST_F(FileUtilsTest, WriteFileSets0666) {
  const mode_t mask = 0000;
  const mode_t mode = 0666;
  const base::FilePath filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = umask(mask);
  EXPECT_TRUE(WriteStringToFile(filename, content));
  int file_mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  umask(old_mask);
}

TEST_F(FileUtilsTest, WriteFileCreatesMissingParentDirectoriesWith0700) {
  const mode_t mask = 0000;
  const mode_t mode = 0700;
  const base::FilePath dirname(GetTempName());
  const base::FilePath subdirname(dirname.Append(GetRandomSuffix()));
  const base::FilePath filename(subdirname.Append(GetRandomSuffix()));
  const std::string content("blablabla");
  EXPECT_TRUE(WriteStringToFile(filename, content));
  int dir_mode = 0;
  int subdir_mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(dirname, &dir_mode));
  EXPECT_TRUE(base::GetPosixFilePermissions(subdirname, &subdir_mode));
  EXPECT_EQ(mode & ~mask, dir_mode & 0777);
  EXPECT_EQ(mode & ~mask, subdir_mode & 0777);
  const mode_t old_mask = umask(mask);
  umask(old_mask);
}

TEST_F(FileUtilsTest, WriteToFileAtomicCanBeReadBack) {
  const base::FilePath filename(GetTempName());
  const std::string content("blablabla");
  EXPECT_TRUE(
      WriteToFileAtomic(filename, content.data(), content.size(), 0644));
  std::string output;
  EXPECT_TRUE(ReadFileToString(filename, &output));
  EXPECT_EQ(content, output);
}

TEST_F(FileUtilsTest, WriteToFileAtomicHonorsMode) {
  const mode_t mask = 0000;
  const mode_t mode = 0616;
  const base::FilePath filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = umask(mask);
  EXPECT_TRUE(
      WriteToFileAtomic(filename, content.data(), content.size(), mode));
  int file_mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  umask(old_mask);
}

TEST_F(FileUtilsTest, WriteToFileAtomicHonorsUmask) {
  const mode_t mask = 0073;
  const mode_t mode = 0777;
  const base::FilePath filename(GetTempName());
  const std::string content("blablabla");
  const mode_t old_mask = umask(mask);
  EXPECT_TRUE(
      WriteToFileAtomic(filename, content.data(), content.size(), mode));
  int file_mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(filename, &file_mode));
  EXPECT_EQ(mode & ~mask, file_mode & 0777);
  umask(old_mask);
}

TEST_F(FileUtilsTest,
       WriteToFileAtomicCreatesMissingParentDirectoriesWith0700) {
  const mode_t mask = 0000;
  const mode_t mode = 0700;
  const base::FilePath dirname(GetTempName());
  const base::FilePath subdirname(dirname.Append(GetRandomSuffix()));
  const base::FilePath filename(subdirname.Append(GetRandomSuffix()));
  const std::string content("blablabla");
  EXPECT_TRUE(
      WriteToFileAtomic(filename, content.data(), content.size(), 0777));
  int dir_mode = 0;
  int subdir_mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(dirname, &dir_mode));
  EXPECT_TRUE(base::GetPosixFilePermissions(subdirname, &subdir_mode));
  EXPECT_EQ(mode & ~mask, dir_mode & 0777);
  EXPECT_EQ(mode & ~mask, subdir_mode & 0777);
  const mode_t old_mask = umask(mask);
  umask(old_mask);
}

}  // namespace brillo
