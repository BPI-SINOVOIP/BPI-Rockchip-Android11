/*
 *  Copyright 2020 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "osi/include/config.h"

#include <base/files/file_util.h>
#include <gtest/gtest.h>

#include <filesystem>

#include "AllocationTestHarness.h"

static const std::filesystem::path kConfigFile =
    std::filesystem::temp_directory_path() / "config_test.conf";
static const char* CONFIG_FILE = kConfigFile.c_str();
static const char CONFIG_FILE_CONTENT[] =
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

class ConfigTest : public AllocationTestHarness {
 protected:
  void SetUp() override {
    AllocationTestHarness::SetUp();
    FILE* fp = fopen(CONFIG_FILE, "wt");
    ASSERT_NE(fp, nullptr);
    ASSERT_EQ(fwrite(CONFIG_FILE_CONTENT, 1, sizeof(CONFIG_FILE_CONTENT), fp),
              sizeof(CONFIG_FILE_CONTENT));
    ASSERT_EQ(fclose(fp), 0);
  }

  void TearDown() override {
    EXPECT_TRUE(std::filesystem::remove(kConfigFile));
    AllocationTestHarness::TearDown();
  }
};

TEST_F(ConfigTest, config_find) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  ASSERT_NE(config, nullptr);
  EXPECT_TRUE(config->Has("DID"));
  auto section_iter = config->Find("DID");
  ASSERT_NE(section_iter, config->sections.end());
  EXPECT_FALSE(config->Has("random"));
  EXPECT_EQ(config->Find("random"), config->sections.end());
}

TEST_F(ConfigTest, section_find) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  ASSERT_NE(config, nullptr);
  EXPECT_TRUE(config->Has("DID"));
  auto section_iter = config->Find("DID");
  ASSERT_NE(section_iter, config->sections.end());
  EXPECT_EQ(section_iter->name, "DID");
  EXPECT_TRUE(section_iter->Has("version"));
  auto entry_iter = section_iter->Find("version");
  ASSERT_NE(entry_iter, section_iter->entries.end());
  EXPECT_EQ(entry_iter->key, "version");
  EXPECT_EQ(entry_iter->value, "0x1436");
  EXPECT_EQ(section_iter->Find("random"), section_iter->entries.end());
  EXPECT_FALSE(section_iter->Has("random"));
}

TEST_F(ConfigTest, section_set) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  ASSERT_NE(config, nullptr);
  EXPECT_TRUE(config->Has("DID"));
  auto section_iter = config->Find("DID");
  ASSERT_NE(section_iter, config->sections.end());
  EXPECT_EQ(section_iter->name, "DID");
  EXPECT_FALSE(section_iter->Has("random"));
  section_iter->Set("random", "foo");
  EXPECT_TRUE(section_iter->Has("random"));
  auto entry_iter = section_iter->Find("random");
  ASSERT_NE(entry_iter, section_iter->entries.end());
  EXPECT_EQ(entry_iter->key, "random");
  EXPECT_EQ(entry_iter->value, "foo");
  section_iter->Set("random", "bar");
  EXPECT_EQ(entry_iter->value, "bar");
  entry_iter = section_iter->Find("random");
  ASSERT_NE(entry_iter, section_iter->entries.end());
  EXPECT_EQ(entry_iter->value, "bar");
}

TEST_F(ConfigTest, config_new_empty) {
  std::unique_ptr<config_t> config = config_new_empty();
  EXPECT_TRUE(config.get() != NULL);
}

TEST_F(ConfigTest, config_new_no_file) {
  std::unique_ptr<config_t> config = config_new("/meow");
  EXPECT_TRUE(config.get() == NULL);
}

TEST_F(ConfigTest, config_new) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config.get() != NULL);
}

TEST_F(ConfigTest, config_new_clone) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  std::unique_ptr<config_t> clone = config_new_clone(*config);

  config_set_string(clone.get(), CONFIG_DEFAULT_SECTION, "first_key",
                    "not_value");

  std::string one = std::string("one");
  EXPECT_STRNE(
      config_get_string(*config, CONFIG_DEFAULT_SECTION, "first_key", &one)
          ->c_str(),
      config_get_string(*clone, CONFIG_DEFAULT_SECTION, "first_key", &one)
          ->c_str());
}

TEST_F(ConfigTest, config_has_section) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_has_section(*config, "DID"));
}

TEST_F(ConfigTest, config_has_key_in_default_section) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_has_key(*config, CONFIG_DEFAULT_SECTION, "first_key"));
  EXPECT_STREQ(
      config_get_string(*config, CONFIG_DEFAULT_SECTION, "first_key", nullptr)
          ->c_str(),
      "value");
}

TEST_F(ConfigTest, config_has_keys) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_has_key(*config, "DID", "recordNumber"));
  EXPECT_TRUE(config_has_key(*config, "DID", "primaryRecord"));
  EXPECT_TRUE(config_has_key(*config, "DID", "productId"));
  EXPECT_TRUE(config_has_key(*config, "DID", "version"));
}

TEST_F(ConfigTest, config_no_bad_keys) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_FALSE(config_has_key(*config, "DID_BAD", "primaryRecord"));
  EXPECT_FALSE(config_has_key(*config, "DID", "primaryRecord_BAD"));
  EXPECT_FALSE(
      config_has_key(*config, CONFIG_DEFAULT_SECTION, "primaryRecord"));
}

TEST_F(ConfigTest, config_get_int_version) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(*config, "DID", "version", 0), 0x1436);
}

TEST_F(ConfigTest, config_get_int_default) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(*config, "DID", "primaryRecord", 123), 123);
}

TEST_F(ConfigTest, config_get_uint64) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_uint64(*config, "DID", "HiSyncId", 0),
            0xFFFFFFFFFFFFFFFF);
  EXPECT_EQ(config_get_uint64(*config, "DID", "HiSyncId2", 0),
            uint64_t(15001900));
}

TEST_F(ConfigTest, config_get_uint64_default) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_uint64(*config, "DID", "primaryRecord", 123),
            uint64_t(123));
}

TEST_F(ConfigTest, config_remove_section) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_remove_section(config.get(), "DID"));
  EXPECT_FALSE(config_has_section(*config, "DID"));
  EXPECT_FALSE(config_has_key(*config, "DID", "productId"));
}

TEST_F(ConfigTest, config_remove_section_missing) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_FALSE(config_remove_section(config.get(), "not a section"));
}

TEST_F(ConfigTest, config_remove_key) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(*config, "DID", "productId", 999), 0x1200);
  EXPECT_TRUE(config_remove_key(config.get(), "DID", "productId"));
  EXPECT_FALSE(config_has_key(*config, "DID", "productId"));
}

TEST_F(ConfigTest, config_remove_key_missing) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(*config, "DID", "productId", 999), 0x1200);
  EXPECT_TRUE(config_remove_key(config.get(), "DID", "productId"));
  EXPECT_EQ(config_get_int(*config, "DID", "productId", 999), 999);
}

TEST_F(ConfigTest, config_save_basic) {
  std::unique_ptr<config_t> config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_save(*config, CONFIG_FILE));
}

TEST_F(ConfigTest, checksum_read) {
  auto tmp_dir = std::filesystem::temp_directory_path();
  auto filename = tmp_dir / "test.checksum";
  std::string checksum = "0x1234";
  base::FilePath file_path(filename.string());

  EXPECT_EQ(base::WriteFile(file_path, checksum.data(), checksum.size()),
            (int)checksum.size());

  EXPECT_EQ(checksum_read(filename.c_str()), checksum.c_str());

  EXPECT_TRUE(std::filesystem::remove(filename));
}

TEST_F(ConfigTest, checksum_save) {
  auto tmp_dir = std::filesystem::temp_directory_path();
  auto filename = tmp_dir / "test.checksum";
  std::string checksum = "0x1234";
  base::FilePath file_path(filename.string());

  EXPECT_TRUE(checksum_save(checksum, filename));

  EXPECT_TRUE(base::PathExists(file_path));

  EXPECT_TRUE(std::filesystem::remove(filename));
}
