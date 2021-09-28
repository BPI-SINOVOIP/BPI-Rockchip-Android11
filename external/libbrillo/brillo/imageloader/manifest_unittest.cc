// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <brillo/imageloader/manifest.h>

namespace brillo {
namespace imageloader {

class ManifestTest : public testing::Test {};

TEST_F(ManifestTest, ParseManifest) {
  const std::string fs_type = R"("ext4")";
  const std::string is_removable = R"(true)";
  const std::string image_sha256_hash =
      R"("4CF41BD11362CCB4707FB93939DBB5AC48745EDFC9DC8D7702852FFAA81B3B3F")";
  const std::string table_sha256_hash =
      R"("0E11DA3D7140C6B95496787F50D15152434EBA22B60443BFA7E054FF4C799276")";
  const std::string version = R"("9824.0.4")";
  const std::string manifest_version = R"(1)";
  const std::string manifest_raw = std::string() + R"(
    {
    "fs-type":)" + fs_type + R"(,
    "is-removable":)" + is_removable +
                                   R"(,
    "image-sha256-hash":)" + image_sha256_hash +
                                   R"(,
    "table-sha256-hash":)" + table_sha256_hash +
                                   R"(,
    "version":)" + version + R"(,
    "manifest-version":)" + manifest_version +
                                   R"(
    }
  )";
  brillo::imageloader::Manifest manifest;
  // Parse the manifest raw string.
  ASSERT_TRUE(manifest.ParseManifest(manifest_raw));
  EXPECT_EQ(manifest.fs_type(), FileSystem::kExt4);
  EXPECT_EQ(manifest.is_removable(), true);
  EXPECT_NE(manifest.image_sha256().size(), 0);
  EXPECT_NE(manifest.table_sha256().size(), 0);
  EXPECT_NE(manifest.version().size(), 0);
  EXPECT_EQ(manifest.manifest_version(), 1);
}

}  // namespace imageloader
}  // namespace brillo
