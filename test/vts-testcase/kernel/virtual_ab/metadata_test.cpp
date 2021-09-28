/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <sys/statfs.h>

#include <android-base/stringprintf.h>
#include <fstab/fstab.h>
#include <gtest/gtest.h>

static constexpr const char kMetadata[] = "/metadata";

TEST(Metadata, IsExt4) {
  struct statfs buf;
  ASSERT_EQ(0, statfs(kMetadata, &buf))
      << "Cannot statfs " << kMetadata << ": " << strerror(errno);
  ASSERT_EQ(EXT4_SUPER_MAGIC, buf.f_type);
}

TEST(Metadata, FstabEntryFlagsAreSet) {
  android::fs_mgr::Fstab fstab;
  ASSERT_TRUE(android::fs_mgr::ReadDefaultFstab(&fstab));

  auto metadata_entry =
      android::fs_mgr::GetEntryForMountPoint(&fstab, kMetadata);
  ASSERT_NE(metadata_entry, nullptr)
      << "Cannot find fstab entry for " << kMetadata;

  const char* message_fmt = "Fstab entry for /metadata must have %s flag set.";

  EXPECT_TRUE(metadata_entry->fs_mgr_flags.check)
      << android::base::StringPrintf(message_fmt, "check");
  EXPECT_TRUE(metadata_entry->fs_mgr_flags.formattable)
      << android::base::StringPrintf(message_fmt, "formattable");
  EXPECT_TRUE(metadata_entry->fs_mgr_flags.first_stage_mount)
      << android::base::StringPrintf(message_fmt, "first_stage_mount");
  EXPECT_TRUE(metadata_entry->fs_mgr_flags.wait)
      << android::base::StringPrintf(message_fmt, "wait");
}
