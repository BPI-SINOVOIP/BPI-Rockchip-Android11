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

#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/scopeguard.h>
#include <gtest/gtest.h>
#include <libavb/libavb.h>
#include <ziparchive/zip_archive.h>

#include "apex_file.h"
#include "apex_preinstalled_data.h"

using android::base::Result;

static std::string testDataDir = android::base::GetExecutableDirectory() + "/";

namespace android {
namespace apex {
namespace {

TEST(ApexFileTest, GetOffsetOfSimplePackage) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_TRUE(apexFile.ok());

  int32_t zip_image_offset;
  size_t zip_image_size;
  {
    ZipArchiveHandle handle;
    int32_t rc = OpenArchive(filePath.c_str(), &handle);
    ASSERT_EQ(0, rc);
    auto close_guard =
        android::base::make_scope_guard([&handle]() { CloseArchive(handle); });

    ZipEntry entry;
    rc = FindEntry(handle, "apex_payload.img", &entry);
    ASSERT_EQ(0, rc);

    zip_image_offset = entry.offset;
    EXPECT_EQ(zip_image_offset % 4096, 0);
    zip_image_size = entry.uncompressed_length;
    EXPECT_EQ(zip_image_size, entry.compressed_length);
  }

  EXPECT_EQ(zip_image_offset, apexFile->GetImageOffset());
  EXPECT_EQ(zip_image_size, apexFile->GetImageSize());
}

TEST(ApexFileTest, GetOffsetMissingFile) {
  const std::string filePath = testDataDir + "missing.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_FALSE(apexFile.ok());
  EXPECT_NE(std::string::npos,
            apexFile.error().message().find("Failed to open package"))
      << apexFile.error();
}

TEST(ApexFileTest, GetApexManifest) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_RESULT_OK(apexFile);
  EXPECT_EQ("com.android.apex.test_package", apexFile->GetManifest().name());
  EXPECT_EQ(1u, apexFile->GetManifest().version());
}

TEST(ApexFileTest, VerifyApexVerity) {
  ASSERT_RESULT_OK(collectPreinstalledData({"/system_ext/apex"}));
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_RESULT_OK(apexFile);

  auto verity_or = apexFile->VerifyApexVerity();
  ASSERT_RESULT_OK(verity_or);

  const ApexVerityData& data = *verity_or;
  EXPECT_NE(nullptr, data.desc.get());
  EXPECT_EQ(std::string("368a22e64858647bc45498e92f749f85482ac468"
                        "50ca7ec8071f49dfa47a243c"),
            data.salt);
  EXPECT_EQ(
      std::string(
          "8e841019e41e8c40bca6dd6304cbf163ea257ba0a268304832c4105eba1c2747"),
      data.root_digest);
}

// TODO: May consider packaging a debug key in debug builds (again).
TEST(ApexFileTest, DISABLED_VerifyApexVerityNoKeyDir) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_RESULT_OK(apexFile);

  auto verity_or = apexFile->VerifyApexVerity();
  ASSERT_FALSE(verity_or.ok());
}

TEST(ApexFileTest, VerifyApexVerityNoKeyInst) {
  const std::string filePath = testDataDir + "apex.apexd_test_no_inst_key.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_RESULT_OK(apexFile);

  auto verity_or = apexFile->VerifyApexVerity();
  ASSERT_FALSE(verity_or.ok());
}

TEST(ApexFileTest, GetBundledPublicKey) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_RESULT_OK(apexFile);

  const std::string keyPath =
      testDataDir + "apexd_testdata/com.android.apex.test_package.avbpubkey";
  std::string keyContent;
  ASSERT_TRUE(android::base::ReadFileToString(keyPath, &keyContent))
      << "Failed to read " << keyPath;

  EXPECT_EQ(keyContent, apexFile->GetBundledPublicKey());
}

TEST(ApexFileTest, CorrutedApex_b146895998) {
  const std::string apex_path = testDataDir + "corrupted_b146895998.apex";
  Result<ApexFile> apex = ApexFile::Open(apex_path);
  ASSERT_RESULT_OK(apex);
  ASSERT_FALSE(apex->VerifyApexVerity());
}

}  // namespace
}  // namespace apex
}  // namespace android
