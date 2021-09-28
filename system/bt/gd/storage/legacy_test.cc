/*
 * Copyright 2020 The Android Open Source Project
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

#include "storage/legacy.h"

#include <gtest/gtest.h>

#include "os/handler.h"
#include "os/thread.h"

#include <base/files/file_util.h>

#ifdef OS_ANDROID
constexpr char CONFIG_FILE[] = "/data/local/tmp/config_test.conf";
#else
constexpr char CONFIG_FILE[] = "/tmp/config_test.conf";
#endif

constexpr char CONFIG_FILE_CONTENT[] =
    "                                                                                \n\
first_key=value                                                                      \n\
                                                                                     \n\
# Device ID (DID) configuration                                                      \n\
[DID]                                                                                \n\
                                                                                     \n\
# Record Number: 1, 2 or 3 - maximum of 3 records                                    \n\
recordNumber = 1                                                                     \n\
                                                                                     \n\
# Primary Record - true or false (default)                                           \n\
# There can be only one primary record                                               \n\
primaryRecord = true                                                                 \n\
                                                                                     \n\
# Vendor ID '0xFFFF' indicates no Device ID Service Record is present in the device  \n\
# 0x000F = Broadcom Corporation (default)                                            \n\
#vendorId = 0x000F                                                                   \n\
                                                                                     \n\
# Vendor ID Source                                                                   \n\
# 0x0001 = Bluetooth SIG assigned Device ID Vendor ID value (default)                \n\
# 0x0002 = USB Implementer's Forum assigned Device ID Vendor ID value                \n\
#vendorIdSource = 0x0001                                                             \n\
                                                                                     \n\
# Product ID & Product Version                                                       \n\
# Per spec DID v1.3 0xJJMN for version is interpreted as JJ.M.N                      \n\
# JJ: major version number, M: minor version number, N: sub-minor version number     \n\
# For example: 1200, v14.3.6                                                         \n\
productId = 0x1200                                                                   \n\
version = 0x1111                                                                     \n\
                                                                                     \n\
# Optional attributes                                                                \n\
#clientExecutableURL =                                                               \n\
#serviceDescription =                                                                \n\
#documentationURL =                                                                  \n\
                                                                                     \n\
# Additional optional DID records. Bluedroid supports up to 3 records.               \n\
[DID]                                                                                \n\
[DID]                                                                                \n\
version = 0x1436                                                                     \n\
                                                                                     \n\
HiSyncId = 18446744073709551615                                                      \n\
HiSyncId2 = 15001900                                                                 \n\
";

namespace bluetooth {
namespace storage {
namespace {

class LegacyStorageTest : public ::testing::Test {
 public:
  void OnConfigRead(std::string filename, std::unique_ptr<config_t> config) {
    promise.set_value();
  }

  void OnConfigWrite(std::string filename, bool success) {
    promise.set_value();
  }

  void OnChecksumRead(std::string filename, std::string hash) {
    promise.set_value();
  }

  void OnChecksumWrite(std::string filename, bool success) {
    promise.set_value();
  }

 protected:
  void SetUp() override {
    handler_ = new os::Handler(&thread_);
    fake_registry_.Start<LegacyModule>(&thread_);
    legacy_module_ = static_cast<LegacyModule*>(fake_registry_.GetModuleUnderTest(&LegacyModule::Factory));
    FILE* fp = fopen(CONFIG_FILE, "wt");
    fwrite(CONFIG_FILE_CONTENT, 1, sizeof(CONFIG_FILE_CONTENT), fp);
    fclose(fp);
  }

  void TearDown() override {
    handler_->Clear();
    fake_registry_.StopAll();
    delete handler_;
  }

  TestModuleRegistry fake_registry_;
  os::Thread& thread_ = fake_registry_.GetTestThread();

  LegacyModule* legacy_module_ = nullptr;
  os::Handler* handler_ = nullptr;
  std::promise<void> promise;
};

TEST_F(LegacyStorageTest, Module) {}

TEST_F(LegacyStorageTest, ConfigRead) {
  std::string filename(CONFIG_FILE);
  auto future = promise.get_future();
  legacy_module_->ConfigRead(filename, common::BindOnce(&LegacyStorageTest::OnConfigRead, common::Unretained(this)),
                             handler_);
  future.wait();
}

TEST_F(LegacyStorageTest, ConfigWrite) {
  std::string filename(CONFIG_FILE);
  config_t config;
  auto future = promise.get_future();
  legacy_module_->ConfigWrite(filename, config,
                              common::BindOnce(&LegacyStorageTest::OnConfigWrite, common::Unretained(this)), handler_);
  future.wait();
}

TEST_F(LegacyStorageTest, ChecksumRead) {
  std::string filename(CONFIG_FILE);
  auto future = promise.get_future();
  legacy_module_->ChecksumRead(filename, common::BindOnce(&LegacyStorageTest::OnChecksumRead, common::Unretained(this)),
                               handler_);
  future.wait();
}

TEST_F(LegacyStorageTest, ChecksumWrite) {
  std::string filename(CONFIG_FILE);
  std::string hash("0123456789abcdef");
  auto future = promise.get_future();
  legacy_module_->ChecksumWrite(
      filename, hash, common::BindOnce(&LegacyStorageTest::OnChecksumWrite, common::Unretained(this)), handler_);
  future.wait();
}

TEST_F(LegacyStorageTest, config_new_empty) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new_empty();
  EXPECT_TRUE(config.get() != NULL);
}

TEST_F(LegacyStorageTest, config_new_no_file) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new("/meow");
  EXPECT_TRUE(config.get() == NULL);
}

TEST_F(LegacyStorageTest, config_new) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_TRUE(config.get() != NULL);
}

TEST_F(LegacyStorageTest, config_new_clone) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  std::unique_ptr<config_t> clone = bluetooth::legacy::osi::config::config_new_clone(*config);

  bluetooth::legacy::osi::config::config_set_string(clone.get(), CONFIG_DEFAULT_SECTION, "first_key", "not_value");

  std::string one = std::string("one");
  EXPECT_STRNE(
      bluetooth::legacy::osi::config::config_get_string(*config, CONFIG_DEFAULT_SECTION, "first_key", &one)->c_str(),
      bluetooth::legacy::osi::config::config_get_string(*clone, CONFIG_DEFAULT_SECTION, "first_key", &one)->c_str());
}

TEST_F(LegacyStorageTest, config_has_section) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_has_section(*config, "DID"));
}

TEST_F(LegacyStorageTest, config_has_key_in_default_section) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_has_key(*config, CONFIG_DEFAULT_SECTION, "first_key"));
  EXPECT_STREQ(
      bluetooth::legacy::osi::config::config_get_string(*config, CONFIG_DEFAULT_SECTION, "first_key", nullptr)->c_str(),
      "value");
}

TEST_F(LegacyStorageTest, config_has_keys) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_has_key(*config, "DID", "recordNumber"));
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_has_key(*config, "DID", "primaryRecord"));
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_has_key(*config, "DID", "productId"));
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_has_key(*config, "DID", "version"));
}

TEST_F(LegacyStorageTest, config_no_bad_keys) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_FALSE(bluetooth::legacy::osi::config::config_has_key(*config, "DID_BAD", "primaryRecord"));
  EXPECT_FALSE(bluetooth::legacy::osi::config::config_has_key(*config, "DID", "primaryRecord_BAD"));
  EXPECT_FALSE(bluetooth::legacy::osi::config::config_has_key(*config, CONFIG_DEFAULT_SECTION, "primaryRecord"));
}

TEST_F(LegacyStorageTest, config_get_int_version) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_int(*config, "DID", "version", 0), 0x1436);
}

TEST_F(LegacyStorageTest, config_get_int_default) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_int(*config, "DID", "primaryRecord", 123), 123);
}

TEST_F(LegacyStorageTest, config_get_uint64) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_uint64(*config, "DID", "HiSyncId", 0), 0xFFFFFFFFFFFFFFFF);
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_uint64(*config, "DID", "HiSyncId2", 0), uint64_t(15001900));
}

TEST_F(LegacyStorageTest, config_get_uint64_default) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_uint64(*config, "DID", "primaryRecord", 123), uint64_t(123));
}

TEST_F(LegacyStorageTest, config_remove_section) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_remove_section(config.get(), "DID"));
  EXPECT_FALSE(bluetooth::legacy::osi::config::config_has_section(*config, "DID"));
  EXPECT_FALSE(bluetooth::legacy::osi::config::config_has_key(*config, "DID", "productId"));
}

TEST_F(LegacyStorageTest, config_remove_section_missing) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_FALSE(bluetooth::legacy::osi::config::config_remove_section(config.get(), "not a section"));
}

TEST_F(LegacyStorageTest, config_remove_key) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_int(*config, "DID", "productId", 999), 0x1200);
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_remove_key(config.get(), "DID", "productId"));
  EXPECT_FALSE(bluetooth::legacy::osi::config::config_has_key(*config, "DID", "productId"));
}

TEST_F(LegacyStorageTest, config_remove_key_missing) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_int(*config, "DID", "productId", 999), 0x1200);
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_remove_key(config.get(), "DID", "productId"));
  EXPECT_EQ(bluetooth::legacy::osi::config::config_get_int(*config, "DID", "productId", 999), 999);
}

TEST_F(LegacyStorageTest, config_save_basic) {
  std::unique_ptr<config_t> config = bluetooth::legacy::osi::config::config_new(CONFIG_FILE);
  EXPECT_TRUE(bluetooth::legacy::osi::config::config_save(*config, CONFIG_FILE));
}

TEST_F(LegacyStorageTest, checksum_read) {
  std::string filename = "/tmp/test.checksum_read";
  std::string checksum = "0x1234";
  base::FilePath file_path(filename);

  EXPECT_EQ(base::WriteFile(file_path, checksum.data(), checksum.size()), (int)checksum.size());

  EXPECT_EQ(bluetooth::legacy::osi::config::checksum_read(filename.c_str()), checksum.c_str());
}

TEST_F(LegacyStorageTest, checksum_save) {
  std::string filename = "/tmp/test.checksum_save";
  std::string checksum = "0x1234";
  base::FilePath file_path(filename);

  EXPECT_TRUE(bluetooth::legacy::osi::config::checksum_save(checksum, filename));

  EXPECT_TRUE(base::PathExists(file_path));
}

}  // namespace
}  // namespace storage
}  // namespace bluetooth
