/*
 * Copyright (C) 2019 The Android Open Source Project
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
#include <tuple>

#include <android-base/macros.h>
#include <gtest/gtest.h>

#include "apex_database.h"

namespace android {
namespace apex {
namespace {

using MountedApexData = MountedApexDatabase::MountedApexData;

TEST(MountedApexDataTest, LinearOrder) {
  constexpr const char* kLoopName[] = {"loop1", "loop2", "loop3"};
  constexpr const char* kPath[] = {"path1", "path2", "path3"};
  constexpr const char* kMount[] = {"mount1", "mount2", "mount3"};
  constexpr const char* kDm[] = {"dm1", "dm2", "dm3"};
  constexpr const char* kHashtreeLoopName[] = {"hash-loop1", "hash-loop2",
                                               "hash-loop3"};
  constexpr size_t kCount = arraysize(kLoopName) * arraysize(kPath) *
                            arraysize(kMount) * arraysize(kDm);

  auto index_fn = [&](size_t i) {
    const size_t loop_index = i % arraysize(kLoopName);
    const size_t loop_rest = i / arraysize(kLoopName);
    const size_t path_index = loop_rest % arraysize(kPath);
    const size_t path_rest = loop_rest / arraysize(kPath);
    const size_t mount_index = path_rest % arraysize(kMount);
    const size_t mount_rest = path_rest / arraysize(kMount);
    const size_t dm_index = mount_rest % arraysize(kDm);
    const size_t dm_rest = mount_rest / arraysize(kHashtreeLoopName);
    const size_t hashtree_loop_index = dm_rest % arraysize(kHashtreeLoopName);
    CHECK_EQ(dm_rest / arraysize(kHashtreeLoopName), 0u);
    return std::make_tuple(loop_index, path_index, mount_index, dm_index,
                           hashtree_loop_index);
  };

  MountedApexData data[kCount];
  for (size_t i = 0; i < kCount; ++i) {
    size_t loop_idx, path_idx, mount_idx, dm_idx, hash_loop_idx;
    std::tie(loop_idx, path_idx, mount_idx, dm_idx, hash_loop_idx) =
        index_fn(i);
    data[i] =
        MountedApexData(kLoopName[loop_idx], kPath[path_idx], kMount[mount_idx],
                        kDm[dm_idx], kHashtreeLoopName[hash_loop_idx]);
  }

  for (size_t i = 0; i < kCount; ++i) {
    size_t loop_idx_i, path_idx_i, mount_idx_i, dm_idx_i, hash_loop_idx_i;
    std::tie(loop_idx_i, path_idx_i, mount_idx_i, dm_idx_i, hash_loop_idx_i) =
        index_fn(i);
    for (size_t j = i; j < kCount; ++j) {
      size_t loop_idx_j, path_idx_j, mount_idx_j, dm_idx_j, hash_loop_idx_j;
      std::tie(loop_idx_j, path_idx_j, mount_idx_j, dm_idx_j, hash_loop_idx_j) =
          index_fn(j);
      if (loop_idx_i != loop_idx_j) {
        EXPECT_EQ(loop_idx_i < loop_idx_j, data[i] < data[j]);
        continue;
      }
      if (path_idx_i != path_idx_j) {
        EXPECT_EQ(path_idx_i < path_idx_j, data[i] < data[j]);
        continue;
      }
      if (mount_idx_i != mount_idx_j) {
        EXPECT_EQ(mount_idx_i < mount_idx_j, data[i] < data[j]);
        continue;
      }
      if (dm_idx_i != dm_idx_j) {
        EXPECT_EQ(dm_idx_i < dm_idx_j, data[i] < data[j]);
        continue;
      }
      EXPECT_EQ(hash_loop_idx_i < hash_loop_idx_j, data[i] < data[j]);
    }
  }
}

size_t CountPackages(const MountedApexDatabase& db) {
  size_t ret = 0;
  db.ForallMountedApexes([&ret](const std::string& a ATTRIBUTE_UNUSED,
                                const MountedApexData& b ATTRIBUTE_UNUSED,
                                bool c ATTRIBUTE_UNUSED) { ++ret; });
  return ret;
}

bool Contains(const MountedApexDatabase& db, const std::string& package,
              const std::string& loop_name, const std::string& full_path,
              const std::string& mount_point, const std::string& device_name,
              const std::string& hashtree_loop_name) {
  bool found = false;
  db.ForallMountedApexes([&](const std::string& p, const MountedApexData& d,
                             bool b ATTRIBUTE_UNUSED) {
    if (package == p && loop_name == d.loop_name && full_path == d.full_path &&
        mount_point == d.mount_point && device_name == d.device_name &&
        hashtree_loop_name == d.hashtree_loop_name) {
      found = true;
    }
  });
  return found;
}

bool ContainsPackage(const MountedApexDatabase& db, const std::string& package,
                     const std::string& loop_name, const std::string& full_path,
                     const std::string& dm,
                     const std::string& hashtree_loop_name) {
  bool found = false;
  db.ForallMountedApexes(
      package, [&](const MountedApexData& d, bool b ATTRIBUTE_UNUSED) {
        if (loop_name == d.loop_name && full_path == d.full_path &&
            dm == d.device_name && hashtree_loop_name == d.hashtree_loop_name) {
          found = true;
        }
      });
  return found;
}

TEST(ApexDatabaseTest, AddRemovedMountedApex) {
  constexpr const char* kPackage = "package";
  constexpr const char* kLoopName = "loop";
  constexpr const char* kPath = "path";
  constexpr const char* kMountPoint = "mount";
  constexpr const char* kDeviceName = "dev";
  constexpr const char* kHashtreeLoopName = "hash-loop";

  MountedApexDatabase db;
  ASSERT_EQ(CountPackages(db), 0u);

  db.AddMountedApex(kPackage, false, kLoopName, kPath, kMountPoint, kDeviceName,
                    kHashtreeLoopName);
  ASSERT_TRUE(Contains(db, kPackage, kLoopName, kPath, kMountPoint, kDeviceName,
                       kHashtreeLoopName));
  ASSERT_TRUE(ContainsPackage(db, kPackage, kLoopName, kPath, kDeviceName,
                              kHashtreeLoopName));

  db.RemoveMountedApex(kPackage, kPath);
  EXPECT_FALSE(Contains(db, kPackage, kLoopName, kPath, kMountPoint,
                        kDeviceName, kHashtreeLoopName));
  EXPECT_FALSE(ContainsPackage(db, kPackage, kLoopName, kPath, kDeviceName,
                               kHashtreeLoopName));
}

TEST(ApexDatabaseTest, MountMultiple) {
  constexpr const char* kPackage[] = {"package", "package", "package",
                                      "package"};
  constexpr const char* kLoopName[] = {"loop", "loop2", "loop3", "loop4"};
  constexpr const char* kPath[] = {"path", "path2", "path", "path4"};
  constexpr const char* kMountPoint[] = {"mount", "mount2", "mount", "mount4"};
  constexpr const char* kDeviceName[] = {"dev", "dev2", "dev3", "dev4"};
  constexpr const char* kHashtreeLoopName[] = {"hash-loop", "hash-loop2",
                                               "hash-loop3", "hash-loop4"};
  MountedApexDatabase db;
  ASSERT_EQ(CountPackages(db), 0u);

  for (size_t i = 0; i < arraysize(kPackage); ++i) {
    db.AddMountedApex(kPackage[i], false, kLoopName[i], kPath[i],
                      kMountPoint[i], kDeviceName[i], kHashtreeLoopName[i]);
  }

  ASSERT_EQ(CountPackages(db), 4u);
  for (size_t i = 0; i < arraysize(kPackage); ++i) {
    ASSERT_TRUE(Contains(db, kPackage[i], kLoopName[i], kPath[i],
                         kMountPoint[i], kDeviceName[i], kHashtreeLoopName[i]));
    ASSERT_TRUE(ContainsPackage(db, kPackage[i], kLoopName[i], kPath[i],
                                kDeviceName[i], kHashtreeLoopName[i]));
  }

  db.RemoveMountedApex(kPackage[0], kPath[0]);
  EXPECT_FALSE(Contains(db, kPackage[0], kLoopName[0], kPath[0], kMountPoint[0],
                        kDeviceName[0], kHashtreeLoopName[0]));
  EXPECT_FALSE(ContainsPackage(db, kPackage[0], kLoopName[0], kPath[0],
                               kDeviceName[0], kHashtreeLoopName[0]));
  EXPECT_TRUE(Contains(db, kPackage[1], kLoopName[1], kPath[1], kMountPoint[1],
                       kDeviceName[1], kHashtreeLoopName[1]));
  EXPECT_TRUE(ContainsPackage(db, kPackage[1], kLoopName[1], kPath[1],
                              kDeviceName[1], kHashtreeLoopName[1]));
  EXPECT_TRUE(Contains(db, kPackage[2], kLoopName[2], kPath[2], kMountPoint[2],
                       kDeviceName[2], kHashtreeLoopName[2]));
  EXPECT_TRUE(ContainsPackage(db, kPackage[2], kLoopName[2], kPath[2],
                              kDeviceName[2], kHashtreeLoopName[2]));
  EXPECT_TRUE(Contains(db, kPackage[3], kLoopName[3], kPath[3], kMountPoint[3],
                       kDeviceName[3], kHashtreeLoopName[3]));
  EXPECT_TRUE(ContainsPackage(db, kPackage[3], kLoopName[3], kPath[3],
                              kDeviceName[3], kHashtreeLoopName[3]));
}

#pragma clang diagnostic push
// error: 'ReturnSentinel' was marked unused but was used
// [-Werror,-Wused-but-marked-unused]
#pragma clang diagnostic ignored "-Wused-but-marked-unused"

TEST(MountedApexDataTest, NoDuplicateLoopDataLoopDevices) {
  ASSERT_DEATH(
      {
        MountedApexDatabase db;
        db.AddMountedApex("package", false, "loop", "path", "mount", "dm",
                          "hashtree-loop1");
        db.AddMountedApex("package2", false, "loop", "path2", "mount2", "dm2",
                          "hashtree-loop2");
      },
      "Duplicate loop device: loop");
}

TEST(MountedApexDataTest, NoDuplicateLoopHashtreeLoopDevices) {
  ASSERT_DEATH(
      {
        MountedApexDatabase db;
        db.AddMountedApex("package", false, "loop1", "path", "mount", "dm",
                          "hashtree-loop");
        db.AddMountedApex("package2", false, "loop2", "path2", "mount2", "dm2",
                          "hashtree-loop");
      },
      "Duplicate loop device: hashtree-loop");
}

TEST(MountedApexDataTest, NoDuplicateLoopHashtreeAndDataLoopDevices) {
  ASSERT_DEATH(
      {
        MountedApexDatabase db;
        db.AddMountedApex("package", false, "loop", "path", "mount", "dm",
                          "hashtree-loop1");
        db.AddMountedApex("package2", false, "loop2", "path2", "mount2", "dm2",
                          "loop");
      },
      "Duplicate loop device: loop");
}

TEST(MountedApexDataTest, NoDuplicateDm) {
  ASSERT_DEATH(
      {
        MountedApexDatabase db;
        db.AddMountedApex("package", false, "loop", "path", "mount", "dm",
                          /* hashtree_loop_name= */ "");
        db.AddMountedApex("package2", false, "loop2", "path2", "mount2", "dm",
                          /* hashtree_loop_name= */ "");
      },
      "Duplicate dm device: dm");
}

#pragma clang diagnostic pop

}  // namespace
}  // namespace apex
}  // namespace android
