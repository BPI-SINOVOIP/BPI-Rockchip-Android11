/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <gtest/gtest.h>
#include <algorithm>
#include <stdio.h>

#include "base/arena_allocator.h"
#include "base/common_art_test.h"
#include "base/unix_file/fd_file.h"
#include "dex/compact_dex_file.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/method_reference.h"
#include "dex/type_reference.h"
#include "profile/profile_compilation_info.h"
#include "ziparchive/zip_writer.h"

namespace art {

using Hotness = ProfileCompilationInfo::MethodHotness;
using ProfileInlineCache = ProfileMethodInfo::ProfileInlineCache;
using ProfileSampleAnnotation = ProfileCompilationInfo::ProfileSampleAnnotation;
using ProfileIndexType = ProfileCompilationInfo::ProfileIndexType;
using ProfileIndexTypeRegular = ProfileCompilationInfo::ProfileIndexTypeRegular;
using ItemMetadata = FlattenProfileData::ItemMetadata;

static constexpr size_t kMaxMethodIds = 65535;
static uint32_t kMaxHotnessFlagBootIndex =
    WhichPowerOf2(static_cast<uint32_t>(Hotness::kFlagLastBoot));
static uint32_t kMaxHotnessFlagRegularIndex =
    WhichPowerOf2(static_cast<uint32_t>(Hotness::kFlagLastRegular));

class ProfileCompilationInfoTest : public CommonArtTest {
 public:
  void SetUp() override {
    CommonArtTest::SetUp();
    allocator_.reset(new ArenaAllocator(&pool_));

    dex1 = fake_dex_storage.AddFakeDex("location1", /* checksum= */ 1, /* num_method_ids= */ 10001);
    dex2 = fake_dex_storage.AddFakeDex("location2", /* checksum= */ 2, /* num_method_ids= */ 10002);
    dex3 = fake_dex_storage.AddFakeDex("location3", /* checksum= */ 3, /* num_method_ids= */ 10003);
    dex4 = fake_dex_storage.AddFakeDex("location4", /* checksum= */ 4, /* num_method_ids= */ 10004);

    dex1_checksum_missmatch = fake_dex_storage.AddFakeDex(
        "location1", /* checksum= */ 12, /* num_method_ids= */ 10001);
    dex1_renamed = fake_dex_storage.AddFakeDex(
        "location1-renamed", /* checksum= */ 1, /* num_method_ids= */ 10001);
    dex2_renamed = fake_dex_storage.AddFakeDex(
        "location2-renamed", /* checksum= */ 2, /* num_method_ids= */ 10002);

    dex_max_methods1 = fake_dex_storage.AddFakeDex(
        "location-max1", /* checksum= */ 5, /* num_method_ids= */ kMaxMethodIds);
    dex_max_methods2 = fake_dex_storage.AddFakeDex(
        "location-max2", /* checksum= */ 6, /* num_method_ids= */ kMaxMethodIds);
  }

 protected:
  bool AddMethod(ProfileCompilationInfo* info,
                 const DexFile* dex,
                 uint16_t method_idx,
                 Hotness::Flag flags = Hotness::kFlagHot,
                 const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    return info->AddMethod(ProfileMethodInfo(MethodReference(dex, method_idx)),
                           flags,
                           annotation);
  }

  bool AddMethod(ProfileCompilationInfo* info,
                const DexFile* dex,
                uint16_t method_idx,
                const std::vector<ProfileInlineCache>& inline_caches,
                const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    return info->AddMethod(
        ProfileMethodInfo(MethodReference(dex, method_idx), inline_caches),
        Hotness::kFlagHot,
        annotation);
  }

  bool AddClass(ProfileCompilationInfo* info,
                const DexFile* dex,
                dex::TypeIndex type_index,
                const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    std::vector<dex::TypeIndex> classes = {type_index};
    return info->AddClassesForDex(dex, classes.begin(), classes.end(), annotation);
  }

  uint32_t GetFd(const ScratchFile& file) {
    return static_cast<uint32_t>(file.GetFd());
  }

  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> GetMethod(
      const ProfileCompilationInfo& info,
      const DexFile* dex,
      uint16_t method_idx,
      const ProfileSampleAnnotation& annotation = ProfileSampleAnnotation::kNone) {
    return info.GetHotMethodInfo(MethodReference(dex, method_idx), annotation);
  }

  // Creates an inline cache which will be destructed at the end of the test.
  ProfileCompilationInfo::InlineCacheMap* CreateInlineCacheMap() {
    used_inline_caches.emplace_back(new ProfileCompilationInfo::InlineCacheMap(
        std::less<uint16_t>(), allocator_->Adapter(kArenaAllocProfile)));
    return used_inline_caches.back().get();
  }

  // Creates the default inline caches used in tests.
  std::vector<ProfileInlineCache> GetTestInlineCaches() {
    std::vector<ProfileInlineCache> inline_caches;
    // Monomorphic
    for (uint16_t dex_pc = 0; dex_pc < 11; dex_pc++) {
      std::vector<TypeReference> types = {TypeReference(dex1, dex::TypeIndex(0))};
      inline_caches.push_back(ProfileInlineCache(dex_pc, /* missing_types*/ false, types));
    }
    // Polymorphic
    for (uint16_t dex_pc = 11; dex_pc < 22; dex_pc++) {
      std::vector<TypeReference> types = {
          TypeReference(dex1, dex::TypeIndex(0)),
          TypeReference(dex2, dex::TypeIndex(1)),
          TypeReference(dex3, dex::TypeIndex(2))};
      inline_caches.push_back(ProfileInlineCache(dex_pc, /* missing_types*/ false, types));
    }
    // Megamorphic
    for (uint16_t dex_pc = 22; dex_pc < 33; dex_pc++) {
      // we need 5 types to make the cache megamorphic
      std::vector<TypeReference> types = {
          TypeReference(dex1, dex::TypeIndex(0)),
          TypeReference(dex1, dex::TypeIndex(1)),
          TypeReference(dex1, dex::TypeIndex(2)),
          TypeReference(dex1, dex::TypeIndex(3)),
          TypeReference(dex1, dex::TypeIndex(4))};
      inline_caches.push_back(ProfileInlineCache(dex_pc, /* missing_types*/ false, types));
    }
    // Missing types
    for (uint16_t dex_pc = 33; dex_pc < 44; dex_pc++) {
      std::vector<TypeReference> types;
      inline_caches.push_back(ProfileInlineCache(dex_pc, /* missing_types*/ true, types));
    }

    return inline_caches;
  }

  void MakeMegamorphic(/*out*/std::vector<ProfileInlineCache>* inline_caches) {
    for (ProfileInlineCache& cache : *inline_caches) {
      uint16_t k = 5;
      while (cache.classes.size() < ProfileCompilationInfo::kIndividualInlineCacheSize) {
        TypeReference type_ref(dex1, dex::TypeIndex(k++));
        if (std::find(cache.classes.begin(), cache.classes.end(), type_ref) ==
            cache.classes.end()) {
          const_cast<std::vector<TypeReference>*>(&cache.classes)->push_back(type_ref);
        }
      }
    }
  }

  void SetIsMissingTypes(/*out*/std::vector<ProfileInlineCache>* inline_caches) {
    for (ProfileInlineCache& cache : *inline_caches) {
      *(const_cast<bool*>(&(cache.is_missing_types))) = true;
    }
  }

  void TestProfileLoadFromZip(const char* zip_entry,
                              size_t zip_flags,
                              bool should_succeed,
                              bool should_succeed_with_empty_profile = false) {
    // Create a valid profile.
    ScratchFile profile;
    ProfileCompilationInfo saved_info;
    for (uint16_t i = 0; i < 10; i++) {
      ASSERT_TRUE(AddMethod(&saved_info, dex1, /* method_idx= */ i));
      ASSERT_TRUE(AddMethod(&saved_info, dex2, /* method_idx= */ i));
    }
    ASSERT_TRUE(saved_info.Save(GetFd(profile)));
    ASSERT_EQ(0, profile.GetFile()->Flush());

    // Prepare the profile content for zipping.
    ASSERT_TRUE(profile.GetFile()->ResetOffset());
    std::vector<uint8_t> data(profile.GetFile()->GetLength());
    ASSERT_TRUE(profile.GetFile()->ReadFully(data.data(), data.size()));

    // Zip the profile content.
    ScratchFile zip;
    FILE* file = fopen(zip.GetFile()->GetPath().c_str(), "wb");
    ZipWriter writer(file);
    writer.StartEntry(zip_entry, zip_flags);
    writer.WriteBytes(data.data(), data.size());
    writer.FinishEntry();
    writer.Finish();
    fflush(file);
    fclose(file);

    // Verify loading from the zip archive.
    ProfileCompilationInfo loaded_info;
    ASSERT_TRUE(zip.GetFile()->ResetOffset());
    ASSERT_EQ(should_succeed, loaded_info.Load(zip.GetFile()->GetPath(), false));
    if (should_succeed) {
      if (should_succeed_with_empty_profile) {
        ASSERT_TRUE(loaded_info.IsEmpty());
      } else {
        ASSERT_TRUE(loaded_info.Equals(saved_info));
      }
    }
  }

  bool IsEmpty(const ProfileCompilationInfo& info) {
    return info.IsEmpty();
  }

  void SizeStressTest(bool random) {
    ProfileCompilationInfo boot_profile(/*for_boot_image*/ true);
    ProfileCompilationInfo reg_profile(/*for_boot_image*/ false);

    static constexpr size_t kNumDexFiles = 5;

    FakeDexStorage local_storage;
    std::vector<const DexFile*> dex_files;
    for (uint32_t i = 0; i < kNumDexFiles; i++) {
      dex_files.push_back(local_storage.AddFakeDex(std::to_string(i), i, kMaxMethodIds));
    }

    std::srand(0);
    // Set a few flags on a 2 different methods in each of the profile.
    for (const DexFile* dex_file : dex_files) {
      for (uint32_t method_idx = 0; method_idx < kMaxMethodIds; method_idx++) {
        for (uint32_t flag_index = 0; flag_index <= kMaxHotnessFlagBootIndex; flag_index++) {
          if (!random || rand() % 2 == 0) {
            ASSERT_TRUE(AddMethod(
                &boot_profile,
                dex_file,
                method_idx,
                static_cast<Hotness::Flag>(1 << flag_index)));
          }
        }
        for (uint32_t flag_index = 0; flag_index <= kMaxHotnessFlagRegularIndex; flag_index++) {
          if (!random || rand() % 2 == 0) {
            ASSERT_TRUE(AddMethod(
                &reg_profile,
                dex_file,
                method_idx,
                static_cast<Hotness::Flag>(1 << flag_index)));
          }
        }
      }
    }

    ScratchFile boot_file;
    ScratchFile reg_file;

    ASSERT_TRUE(boot_profile.Save(GetFd(boot_file)));
    ASSERT_TRUE(reg_profile.Save(GetFd(reg_file)));
    ASSERT_TRUE(boot_file.GetFile()->ResetOffset());
    ASSERT_TRUE(reg_file.GetFile()->ResetOffset());

    ProfileCompilationInfo loaded_boot;
    ProfileCompilationInfo loaded_reg;
    ASSERT_TRUE(loaded_boot.Load(GetFd(boot_file)));
    ASSERT_TRUE(loaded_reg.Load(GetFd(reg_file)));
  }

  // Cannot sizeof the actual arrays so hard code the values here.
  // They should not change anyway.
  static constexpr int kProfileMagicSize = 4;
  static constexpr int kProfileVersionSize = 4;

  MallocArenaPool pool_;
  std::unique_ptr<ArenaAllocator> allocator_;

  const DexFile* dex1;
  const DexFile* dex2;
  const DexFile* dex3;
  const DexFile* dex4;
  const DexFile* dex1_checksum_missmatch;
  const DexFile* dex1_renamed;
  const DexFile* dex2_renamed;
  const DexFile* dex_max_methods1;
  const DexFile* dex_max_methods2;

  // Cache of inline caches generated during tests.
  // This makes it easier to pass data between different utilities and ensure that
  // caches are destructed at the end of the test.
  std::vector<std::unique_ptr<ProfileCompilationInfo::InlineCacheMap>> used_inline_caches;

  FakeDexStorage fake_dex_storage;
};

TEST_F(ProfileCompilationInfoTest, SaveFd) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  // Save a few methods.
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod(&saved_info, dex1, /* method_idx= */ i));
    ASSERT_TRUE(AddMethod(&saved_info, dex2, /* method_idx= */ i));
  }
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(saved_info));

  // Save more methods.
  for (uint16_t i = 0; i < 100; i++) {
    ASSERT_TRUE(AddMethod(&saved_info, dex1, /* method_idx= */ i));
    ASSERT_TRUE(AddMethod(&saved_info, dex2, /* method_idx= */ i));
    ASSERT_TRUE(AddMethod(&saved_info, dex3, /* method_idx= */ i));
  }
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back everything we saved.
  ProfileCompilationInfo loaded_info2;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info2.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info2.Equals(saved_info));
}

TEST_F(ProfileCompilationInfoTest, AddMethodsAndClassesFail) {
  ScratchFile profile;

  ProfileCompilationInfo info;
  ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ 1));
  // Trying to add info for an existing file but with a different checksum.
  ASSERT_FALSE(AddMethod(&info, dex1_checksum_missmatch, /* method_idx= */ 2));
}

TEST_F(ProfileCompilationInfoTest, MergeFail) {
  ScratchFile profile;

  ProfileCompilationInfo info1;
  ASSERT_TRUE(AddMethod(&info1, dex1, /* method_idx= */ 1));
  // Use the same file, change the checksum.
  ProfileCompilationInfo info2;
  ASSERT_TRUE(AddMethod(&info2, dex1_checksum_missmatch, /* method_idx= */ 2));

  ASSERT_FALSE(info1.MergeWith(info2));
}


TEST_F(ProfileCompilationInfoTest, MergeFdFail) {
  ScratchFile profile;

  ProfileCompilationInfo info1;
  ASSERT_TRUE(AddMethod(&info1, dex1, /* method_idx= */ 1));
  // Use the same file, change the checksum.
  ProfileCompilationInfo info2;
  ASSERT_TRUE(AddMethod(&info2, dex1_checksum_missmatch, /* method_idx= */ 2));

  ASSERT_TRUE(info1.Save(profile.GetFd()));
  ASSERT_EQ(0, profile.GetFile()->Flush());
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  ASSERT_FALSE(info2.Load(profile.GetFd()));
}

TEST_F(ProfileCompilationInfoTest, SaveMaxMethods) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  // Save the maximum number of methods
  for (uint16_t i = 0; i < std::numeric_limits<uint16_t>::max(); i++) {
    ASSERT_TRUE(AddMethod(&saved_info, dex_max_methods1, /* method_idx= */ i));
    ASSERT_TRUE(AddMethod(&saved_info, dex_max_methods2, /* method_idx= */ i));
  }
  // Save the maximum number of classes
  for (uint16_t i = 0; i < std::numeric_limits<uint16_t>::max(); i++) {
    ASSERT_TRUE(AddClass(&saved_info, dex1, dex::TypeIndex(i)));
    ASSERT_TRUE(AddClass(&saved_info, dex2, dex::TypeIndex(i)));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(saved_info));
}

TEST_F(ProfileCompilationInfoTest, SaveEmpty) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(saved_info));
}

TEST_F(ProfileCompilationInfoTest, LoadEmpty) {
  ScratchFile profile;

  ProfileCompilationInfo empty_info;

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(empty_info));
}

TEST_F(ProfileCompilationInfoTest, BadMagic) {
  ScratchFile profile;
  uint8_t buffer[] = { 1, 2, 3, 4 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(buffer, sizeof(buffer)));
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, BadVersion) {
  ScratchFile profile;

  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  uint8_t version[] = { 'v', 'e', 'r', 's', 'i', 'o', 'n' };
  ASSERT_TRUE(profile.GetFile()->WriteFully(version, sizeof(version)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, Incomplete) {
  ScratchFile profile;
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileVersion, kProfileVersionSize));
  // Write that we have at least one line.
  uint8_t line_number[] = { 0, 1 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line_number, sizeof(line_number)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, TooLongDexLocation) {
  ScratchFile profile;
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileVersion, kProfileVersionSize));
  // Write that we have at least one line.
  uint8_t line_number[] = { 0, 1 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line_number, sizeof(line_number)));

  // dex_location_size, methods_size, classes_size, checksum.
  // Dex location size is too big and should be rejected.
  uint8_t line[] = { 255, 255, 0, 1, 0, 1, 0, 0, 0, 0 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line, sizeof(line)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, UnexpectedContent) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod(&saved_info, dex1, /* method_idx= */ i));
  }
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));

  uint8_t random_data[] = { 1, 2, 3};
  ASSERT_TRUE(profile.GetFile()->WriteFully(random_data, sizeof(random_data)));

  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we fail because of unexpected data at the end of the file.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, SaveInlineCaches) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  std::vector<ProfileInlineCache> inline_caches = GetTestInlineCaches();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the
    // class from the inline caches.
    ASSERT_TRUE(AddMethod(&saved_info, dex1, method_idx, inline_caches));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod(&saved_info, dex4, method_idx, inline_caches));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));

  ASSERT_TRUE(loaded_info.Equals(saved_info));

  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
      GetMethod(loaded_info, dex1, /* method_idx= */ 3);
  ASSERT_TRUE(loaded_pmi1 != nullptr);
  ASSERT_TRUE(*loaded_pmi1 == inline_caches);
  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi2 =
      GetMethod(loaded_info, dex4, /* method_idx= */ 3);
  ASSERT_TRUE(loaded_pmi2 != nullptr);
  ASSERT_TRUE(*loaded_pmi2 == inline_caches);
}

TEST_F(ProfileCompilationInfoTest, MegamorphicInlineCaches) {
  ProfileCompilationInfo saved_info;
  std::vector<ProfileInlineCache> inline_caches = GetTestInlineCaches();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(&saved_info, dex1, method_idx, inline_caches));
  }

  ScratchFile profile;
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Make the inline caches megamorphic and add them to the profile again.
  ProfileCompilationInfo saved_info_extra;
  std::vector<ProfileInlineCache> inline_caches_extra = GetTestInlineCaches();
  MakeMegamorphic(&inline_caches_extra);
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(&saved_info_extra, dex1, method_idx, inline_caches_extra));
  }

  ScratchFile extra_profile;
  ASSERT_TRUE(saved_info_extra.Save(GetFd(extra_profile)));
  ASSERT_EQ(0, extra_profile.GetFile()->Flush());

  // Merge the profiles so that we have the same view as the file.
  ASSERT_TRUE(saved_info.MergeWith(saved_info_extra));

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(extra_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(extra_profile)));

  ASSERT_TRUE(loaded_info.Equals(saved_info));

  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
      GetMethod(loaded_info, dex1, /* method_idx= */ 3);

  ASSERT_TRUE(loaded_pmi1 != nullptr);
  ASSERT_TRUE(*loaded_pmi1 == inline_caches_extra);
}

TEST_F(ProfileCompilationInfoTest, MissingTypesInlineCaches) {
  ProfileCompilationInfo saved_info;
  std::vector<ProfileInlineCache> inline_caches = GetTestInlineCaches();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(&saved_info, dex1, method_idx, inline_caches));
  }

  ScratchFile profile;
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Make some inline caches megamorphic and add them to the profile again.
  ProfileCompilationInfo saved_info_extra;
  std::vector<ProfileInlineCache> inline_caches_extra = GetTestInlineCaches();
  MakeMegamorphic(&inline_caches_extra);
  for (uint16_t method_idx = 5; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(&saved_info_extra, dex1, method_idx, inline_caches));
  }

  // Mark all inline caches with missing types and add them to the profile again.
  // This will verify that all inline caches (megamorphic or not) should be marked as missing types.
  std::vector<ProfileInlineCache> missing_types = GetTestInlineCaches();
  SetIsMissingTypes(&missing_types);
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(&saved_info_extra, dex1, method_idx, missing_types));
  }

  ScratchFile extra_profile;
  ASSERT_TRUE(saved_info_extra.Save(GetFd(extra_profile)));
  ASSERT_EQ(0, extra_profile.GetFile()->Flush());

  // Merge the profiles so that we have the same view as the file.
  ASSERT_TRUE(saved_info.MergeWith(saved_info_extra));

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(extra_profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(extra_profile)));

  ASSERT_TRUE(loaded_info.Equals(saved_info));

  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
      GetMethod(loaded_info, dex1, /* method_idx= */ 3);
  ASSERT_TRUE(loaded_pmi1 != nullptr);
  ASSERT_TRUE(*loaded_pmi1 == missing_types);
}

TEST_F(ProfileCompilationInfoTest, InvalidChecksumInInlineCache) {
  ScratchFile profile;

  ProfileCompilationInfo info;
  std::vector<ProfileInlineCache> inline_caches1 = GetTestInlineCaches();
  std::vector<ProfileInlineCache> inline_caches2 = GetTestInlineCaches();
  // Modify the checksum to trigger a mismatch.
  std::vector<TypeReference>* types = const_cast<std::vector<TypeReference>*>(
      &inline_caches2[0].classes);
  types->front().dex_file = dex1_checksum_missmatch;

  ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ 0, inline_caches1));
  ASSERT_FALSE(AddMethod(&info, dex2, /* method_idx= */ 0, inline_caches2));
}

// Verify that profiles behave correctly even if the methods are added in a different
// order and with a different dex profile indices for the dex files.
TEST_F(ProfileCompilationInfoTest, MergeInlineCacheTriggerReindex) {
  ScratchFile profile;

  ProfileCompilationInfo info;
  ProfileCompilationInfo info_reindexed;

  std::vector<ProfileInlineCache> inline_caches;
  for (uint16_t dex_pc = 1; dex_pc < 5; dex_pc++) {
    std::vector<TypeReference> types = {
        TypeReference(dex1, dex::TypeIndex(0)),
        TypeReference(dex2, dex::TypeIndex(1))};
    inline_caches.push_back(ProfileInlineCache(dex_pc, /* missing_types*/ false, types));
  }

  std::vector<ProfileInlineCache> inline_caches_reindexed;
  for (uint16_t dex_pc = 1; dex_pc < 5; dex_pc++) {
    std::vector<TypeReference> types = {
        TypeReference(dex2, dex::TypeIndex(1)),
        TypeReference(dex1, dex::TypeIndex(0))};
    inline_caches_reindexed.push_back(ProfileInlineCache(dex_pc, /* missing_types*/ false, types));
  }
  // Profile 1 and Profile 2 get the same methods but in different order.
  // This will trigger a different dex numbers.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(&info, dex1, method_idx, inline_caches));
    ASSERT_TRUE(AddMethod(&info, dex2, method_idx, inline_caches));
  }

  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(&info_reindexed, dex2, method_idx, inline_caches_reindexed));
    ASSERT_TRUE(AddMethod(&info_reindexed, dex1, method_idx, inline_caches_reindexed));
  }

  ProfileCompilationInfo info_backup;
  info_backup.MergeWith(info);
  ASSERT_TRUE(info.MergeWith(info_reindexed));
  // Merging should have no effect as we're adding the exact same stuff.
  ASSERT_TRUE(info.Equals(info_backup));
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
        GetMethod(info, dex1, method_idx);
    ASSERT_TRUE(loaded_pmi1 != nullptr);
    ASSERT_TRUE(*loaded_pmi1 == inline_caches);
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi2 =
        GetMethod(info, dex2, method_idx);
    ASSERT_TRUE(loaded_pmi2 != nullptr);
    ASSERT_TRUE(*loaded_pmi2 == inline_caches);
  }
}

TEST_F(ProfileCompilationInfoTest, AddMoreDexFileThanLimitRegular) {
  FakeDexStorage local_storage;
  ProfileCompilationInfo info;
  // Save a few methods.
  for (uint16_t i = 0; i < std::numeric_limits<ProfileIndexTypeRegular>::max(); i++) {
    std::string location = std::to_string(i);
    const DexFile* dex = local_storage.AddFakeDex(
        location, /* checksum= */ 1, /* num_method_ids= */ 1);
    ASSERT_TRUE(AddMethod(&info, dex, /* method_idx= */ 0));
  }
  // Add an extra dex file.
  const DexFile* dex = local_storage.AddFakeDex("-1", /* checksum= */ 1, /* num_method_ids= */ 1);
  ASSERT_FALSE(AddMethod(&info, dex, /* method_idx= */ 0));
}

TEST_F(ProfileCompilationInfoTest, AddMoreDexFileThanLimitBoot) {
  FakeDexStorage local_storage;
  ProfileCompilationInfo info(/*for_boot_image=*/true);
  // Save a few methods.
  for (uint16_t i = 0; i < std::numeric_limits<ProfileIndexType>::max(); i++) {
    std::string location = std::to_string(i);
    const DexFile* dex = local_storage.AddFakeDex(
        location, /* checksum= */ 1, /* num_method_ids= */ 1);
    ASSERT_TRUE(AddMethod(&info, dex, /* method_idx= */ 0));
  }
  // Add an extra dex file.
  const DexFile* dex = local_storage.AddFakeDex("-1", /* checksum= */ 1, /* num_method_ids= */ 1);
  ASSERT_FALSE(AddMethod(&info, dex, /* method_idx= */ 0));
}

TEST_F(ProfileCompilationInfoTest, MegamorphicInlineCachesMerge) {
  // Create a megamorphic inline cache.
  std::vector<ProfileInlineCache> inline_caches;
  std::vector<TypeReference> types = {
          TypeReference(dex1, dex::TypeIndex(0)),
          TypeReference(dex1, dex::TypeIndex(1)),
          TypeReference(dex1, dex::TypeIndex(2)),
          TypeReference(dex1, dex::TypeIndex(3)),
          TypeReference(dex1, dex::TypeIndex(4))};
  inline_caches.push_back(ProfileInlineCache(0, /* missing_types*/ false, types));

  ProfileCompilationInfo info_megamorphic;
  ASSERT_TRUE(AddMethod(&info_megamorphic, dex1, 0, inline_caches));

  // Create a profile with no inline caches (for the same method).
  ProfileCompilationInfo info_no_inline_cache;
  ASSERT_TRUE(AddMethod(&info_no_inline_cache, dex1, 0));

  // Merge the megamorphic cache into the empty one.
  ASSERT_TRUE(info_no_inline_cache.MergeWith(info_megamorphic));
  ScratchFile profile;
  // Saving profile should work without crashing (b/35644850).
  ASSERT_TRUE(info_no_inline_cache.Save(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, MissingTypesInlineCachesMerge) {
  // Create an inline cache with missing types
  std::vector<ProfileInlineCache> inline_caches;
  std::vector<TypeReference> types = {};
  inline_caches.push_back(ProfileInlineCache(0, /* missing_types*/ true, types));

  ProfileCompilationInfo info_missing_types;
  ASSERT_TRUE(AddMethod(&info_missing_types, dex1, /*method_idx=*/ 0, inline_caches));

  // Create a profile with no inline caches (for the same method).
  ProfileCompilationInfo info_no_inline_cache;
  ASSERT_TRUE(AddMethod(&info_no_inline_cache, dex1, /*method_idx=*/ 0));

  // Merge the missing type cache into the empty one.
  // Everything should be saved without errors.
  ASSERT_TRUE(info_no_inline_cache.MergeWith(info_missing_types));
  ScratchFile profile;
  ASSERT_TRUE(info_no_inline_cache.Save(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, SampledMethodsTest) {
  ProfileCompilationInfo test_info;
  AddMethod(&test_info, dex1, 1, Hotness::kFlagStartup);
  AddMethod(&test_info, dex1, 5, Hotness::kFlagPostStartup);
  AddMethod(&test_info, dex2, 2, Hotness::kFlagStartup);
  AddMethod(&test_info, dex2, 4, Hotness::kFlagPostStartup);
  auto run_test = [&dex1 = dex1, &dex2 = dex2](const ProfileCompilationInfo& info) {
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, 2)).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, 4)).IsInProfile());
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 1)).IsStartup());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, 3)).IsStartup());
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 5)).IsPostStartup());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, 6)).IsStartup());
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex2, 2)).IsStartup());
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex2, 4)).IsPostStartup());
  };
  run_test(test_info);

  // Save the profile.
  ScratchFile profile;
  ASSERT_TRUE(test_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Load the profile and make sure we can read the data and it matches what we expect.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  run_test(loaded_info);

  // Test that the bitmap gets merged properly.
  EXPECT_FALSE(test_info.GetMethodHotness(MethodReference(dex1, 11)).IsStartup());
  {
    ProfileCompilationInfo merge_info;
    AddMethod(&merge_info, dex1, 11, Hotness::kFlagStartup);
    test_info.MergeWith(merge_info);
  }
  EXPECT_TRUE(test_info.GetMethodHotness(MethodReference(dex1, 11)).IsStartup());

  // Test bulk adding.
  {
    std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
    ProfileCompilationInfo info;
    std::vector<uint16_t> hot_methods = {1, 3, 5};
    std::vector<uint16_t> startup_methods = {1, 2};
    std::vector<uint16_t> post_methods = {0, 2, 6};
    ASSERT_GE(dex->NumMethodIds(), 7u);
    info.AddMethodsForDex(static_cast<Hotness::Flag>(Hotness::kFlagHot | Hotness::kFlagStartup),
                          dex.get(),
                          hot_methods.begin(),
                          hot_methods.end());
    info.AddMethodsForDex(Hotness::kFlagStartup,
                          dex.get(),
                          startup_methods.begin(),
                          startup_methods.end());
    info.AddMethodsForDex(Hotness::kFlagPostStartup,
                          dex.get(),
                          post_methods.begin(),
                          post_methods.end());
    for (uint16_t id : hot_methods) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsHot());
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsStartup());
    }
    for (uint16_t id : startup_methods) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsStartup());
    }
    for (uint16_t id : post_methods) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsPostStartup());
    }
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), 6)).IsPostStartup());
    // Check that methods that shouldn't have been touched are OK.
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), 0)).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 4)).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 7)).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 1)).IsPostStartup());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 4)).IsStartup());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 6)).IsStartup());
  }
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipCompress) {
  TestProfileLoadFromZip("primary.prof",
                         ZipWriter::kCompress | ZipWriter::kAlign32,
                         /*should_succeed=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipUnCompress) {
  TestProfileLoadFromZip("primary.prof",
                         ZipWriter::kAlign32,
                         /*should_succeed=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipUnAligned) {
  TestProfileLoadFromZip("primary.prof",
                         0,
                         /*should_succeed=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipFailBadZipEntry) {
  TestProfileLoadFromZip("invalid.profile.entry",
                         0,
                         /*should_succeed=*/true,
                         /*should_succeed_with_empty_profile=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipFailBadProfile) {
  // Create a bad profile.
  ScratchFile profile;
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileVersion, kProfileVersionSize));
  // Write that we have at least one line.
  uint8_t line_number[] = { 0, 1 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line_number, sizeof(line_number)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Prepare the profile content for zipping.
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  std::vector<uint8_t> data(profile.GetFile()->GetLength());
  ASSERT_TRUE(profile.GetFile()->ReadFully(data.data(), data.size()));

  // Zip the profile content.
  ScratchFile zip;
  FILE* file = fopen(zip.GetFile()->GetPath().c_str(), "wb");
  ZipWriter writer(file);
  writer.StartEntry("primary.prof", ZipWriter::kAlign32);
  writer.WriteBytes(data.data(), data.size());
  writer.FinishEntry();
  writer.Finish();
  fflush(file);
  fclose(file);

  // Check that we failed to load.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(zip.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(zip)));
}

TEST_F(ProfileCompilationInfoTest, UpdateProfileKeyOk) {
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files.push_back(std::unique_ptr<const DexFile>(dex1_renamed));
  dex_files.push_back(std::unique_ptr<const DexFile>(dex2_renamed));

  ProfileCompilationInfo info;
  AddMethod(&info, dex1, /* method_idx= */ 0);
  AddMethod(&info, dex2, /* method_idx= */ 0);

  // Update the profile keys based on the original dex files
  ASSERT_TRUE(info.UpdateProfileKeys(dex_files));

  // Verify that we find the methods when searched with the original dex files.
  for (const std::unique_ptr<const DexFile>& dex : dex_files) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi =
        GetMethod(info, dex.get(), /* method_idx= */ 0);
    ASSERT_TRUE(loaded_pmi != nullptr);
  }

  // Release the ownership as this is held by the test class;
  for (std::unique_ptr<const DexFile>& dex : dex_files) {
    UNUSED(dex.release());
  }
}

TEST_F(ProfileCompilationInfoTest, UpdateProfileKeyOkButNoUpdate) {
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files.push_back(std::unique_ptr<const DexFile>(dex1));

  ProfileCompilationInfo info;
  AddMethod(&info, dex2, /* method_idx= */ 0);

  // Update the profile keys based on the original dex files.
  ASSERT_TRUE(info.UpdateProfileKeys(dex_files));

  // Verify that we did not perform any update and that we cannot find anything with the new
  // location.
  for (const std::unique_ptr<const DexFile>& dex : dex_files) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi =
        GetMethod(info, dex.get(), /* method_idx= */ 0);
    ASSERT_TRUE(loaded_pmi == nullptr);
  }

  // Verify that we can find the original entry.
  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi =
        GetMethod(info, dex2, /* method_idx= */ 0);
  ASSERT_TRUE(loaded_pmi != nullptr);

  // Release the ownership as this is held by the test class;
  for (std::unique_ptr<const DexFile>& dex : dex_files) {
    UNUSED(dex.release());
  }
}

TEST_F(ProfileCompilationInfoTest, UpdateProfileKeyFail) {
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files.push_back(std::unique_ptr<const DexFile>(dex1_renamed));

  ProfileCompilationInfo info;
  AddMethod(&info, dex1, /* method_idx= */ 0);

  // Add a method index using the location we want to rename to.
  // This will cause the rename to fail because an existing entry would already have that name.
  AddMethod(&info, dex1_renamed, /* method_idx= */ 0);

  ASSERT_FALSE(info.UpdateProfileKeys(dex_files));

  // Release the ownership as this is held by the test class;
  for (std::unique_ptr<const DexFile>& dex : dex_files) {
    UNUSED(dex.release());
  }
}

TEST_F(ProfileCompilationInfoTest, FilteredLoading) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  std::vector<ProfileInlineCache> inline_caches = GetTestInlineCaches();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the class from the inline caches.
    ASSERT_TRUE(AddMethod(&saved_info, dex1, method_idx, inline_caches));
    ASSERT_TRUE(AddMethod(&saved_info, dex2, method_idx, inline_caches));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod(&saved_info, dex4, method_idx, inline_caches));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Filter out dex locations. Keep only dex_location1 and dex_location3.
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [&dex1 = dex1, &dex3 = dex3](const std::string& dex_location, uint32_t checksum) -> bool {
          return (dex_location == dex1->GetLocation() && checksum == dex1->GetLocationChecksum())
              || (dex_location == dex3->GetLocation() && checksum == dex3->GetLocationChecksum());
        };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));

  // Verify that we filtered out locations during load.

  // Dex location 2 and 4 should have been filtered out
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(nullptr == GetMethod(loaded_info, dex2, method_idx));
    ASSERT_TRUE(nullptr == GetMethod(loaded_info, dex4, method_idx));
  }

  // Dex location 1 should have all all the inline caches referencing dex location 2 set to
  // missing types.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // The methods for dex location 1 should be in the profile data.
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
        GetMethod(loaded_info, dex1, method_idx);
    ASSERT_TRUE(loaded_pmi1 != nullptr);

    // Verify the inline cache.
    // Everything should be as constructed by GetTestInlineCaches with the exception
    // of the inline caches referring types from dex_location2.
    // These should be set to IsMissingType.
    ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();

    // Monomorphic types should remain the same as dex_location1 was kept.
    for (uint16_t dex_pc = 0; dex_pc < 11; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.AddClass(0, dex::TypeIndex(0));
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Polymorphic inline cache should have been transformed to IsMissingType due to
    // the removal of dex_location2.
    for (uint16_t dex_pc = 11; dex_pc < 22; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMissingTypes();
      ic_map->Put(dex_pc, dex_pc_data);
    }

    // Megamorphic are not affected by removal of dex files.
    for (uint16_t dex_pc = 22; dex_pc < 33; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMegamorphic();
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Missing types are not affected be removal of dex files.
    for (uint16_t dex_pc = 33; dex_pc < 44; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMissingTypes();
      ic_map->Put(dex_pc, dex_pc_data);
    }

    ProfileCompilationInfo::OfflineProfileMethodInfo expected_pmi(ic_map);

    // The dex references should not have  dex_location2 in the list.
    expected_pmi.dex_references.emplace_back(
        dex1->GetLocation(), dex1->GetLocationChecksum(), dex1->NumMethodIds());
    expected_pmi.dex_references.emplace_back(
        dex3->GetLocation(), dex3->GetLocationChecksum(), dex3->NumMethodIds());

    // Now check that we get back what we expect.
    ASSERT_TRUE(*loaded_pmi1 == expected_pmi);
  }
}

TEST_F(ProfileCompilationInfoTest, FilteredLoadingRemoveAll) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  std::vector<ProfileInlineCache> inline_caches = GetTestInlineCaches();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the class from the inline caches.
    ASSERT_TRUE(AddMethod(&saved_info, dex1, method_idx, inline_caches));
    ASSERT_TRUE(AddMethod(&saved_info, dex2, method_idx, inline_caches));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod(&saved_info, dex4, method_idx, inline_caches));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Remove all elements.
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [](const std::string&, uint32_t) -> bool { return false; };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));

  // Verify that we filtered out everything.
  ASSERT_TRUE(IsEmpty(loaded_info));
}

TEST_F(ProfileCompilationInfoTest, FilteredLoadingKeepAll) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  std::vector<ProfileInlineCache> inline_caches = GetTestInlineCaches();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the
    // class from the inline caches.
    ASSERT_TRUE(AddMethod(&saved_info, dex1, method_idx, inline_caches));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod(&saved_info, dex4, method_idx, inline_caches));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Keep all elements.
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [](const std::string&, uint32_t) -> bool { return true; };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));


  ASSERT_TRUE(loaded_info.Equals(saved_info));

  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
        GetMethod(loaded_info, dex1, method_idx);
    ASSERT_TRUE(loaded_pmi1 != nullptr);
    ASSERT_TRUE(*loaded_pmi1 == inline_caches);
  }
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi2 =
        GetMethod(loaded_info, dex4, method_idx);
    ASSERT_TRUE(loaded_pmi2 != nullptr);
    ASSERT_TRUE(*loaded_pmi2 == inline_caches);
  }
}

// Regression test: we were failing to do a filtering loading when the filtered dex file
// contained profiled classes.
TEST_F(ProfileCompilationInfoTest, FilteredLoadingWithClasses) {
  ScratchFile profile;

  // Save a profile with 2 dex files containing just classes.
  ProfileCompilationInfo saved_info;
  uint16_t item_count = 1000;
  for (uint16_t i = 0; i < item_count; i++) {
    ASSERT_TRUE(AddClass(&saved_info, dex1, dex::TypeIndex(i)));
    ASSERT_TRUE(AddClass(&saved_info, dex2, dex::TypeIndex(i)));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());


  // Filter out dex locations: kepp only dex_location2.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [&dex2 = dex2](const std::string& dex_location, uint32_t checksum) -> bool {
          return (dex_location == dex2->GetLocation() && checksum == dex2->GetLocationChecksum());
        };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));

  // Compute the expectation.
  ProfileCompilationInfo expected_info;
  for (uint16_t i = 0; i < item_count; i++) {
    ASSERT_TRUE(AddClass(&expected_info, dex2, dex::TypeIndex(i)));
  }

  // Validate the expectation.
  ASSERT_TRUE(loaded_info.Equals(expected_info));
}


TEST_F(ProfileCompilationInfoTest, ClearData) {
  ProfileCompilationInfo info;
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i));
  }
  ASSERT_FALSE(IsEmpty(info));
  info.ClearData();
  ASSERT_TRUE(IsEmpty(info));
}

TEST_F(ProfileCompilationInfoTest, ClearDataAndSave) {
  ProfileCompilationInfo info;
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i));
  }
  info.ClearData();

  ScratchFile profile;
  ASSERT_TRUE(info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(info));
}

TEST_F(ProfileCompilationInfoTest, InitProfiles) {
  ProfileCompilationInfo info;
  ASSERT_EQ(
      memcmp(info.GetVersion(),
             ProfileCompilationInfo::kProfileVersion,
             ProfileCompilationInfo::kProfileVersionSize),
      0);
  ASSERT_FALSE(info.IsForBootImage());

  ProfileCompilationInfo info1(/*for_boot_image=*/ true);

  ASSERT_EQ(
      memcmp(info1.GetVersion(),
             ProfileCompilationInfo::kProfileVersionForBootImage,
             ProfileCompilationInfo::kProfileVersionSize),
      0);
  ASSERT_TRUE(info1.IsForBootImage());
}

TEST_F(ProfileCompilationInfoTest, VersionEquality) {
  ProfileCompilationInfo info(/*for_boot_image=*/ false);
  ProfileCompilationInfo info1(/*for_boot_image=*/ true);
  ASSERT_FALSE(info.Equals(info1));
}

TEST_F(ProfileCompilationInfoTest, AllMethodFlags) {
  ProfileCompilationInfo info(/*for_boot_image*/ true);

  for (uint32_t index = 0; index <= kMaxHotnessFlagBootIndex; index++) {
    AddMethod(&info, dex1, index, static_cast<Hotness::Flag>(1 << index));
  }

  auto run_test = [&dex1 = dex1](const ProfileCompilationInfo& info) {
    for (uint32_t index = 0; index <= kMaxHotnessFlagBootIndex; index++) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, index)).IsInProfile());
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, index))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index))) << index << " "
            << info.GetMethodHotness(MethodReference(dex1, index)).GetFlags();
    }
  };
  run_test(info);

  // Save the profile.
  ScratchFile profile;
  ASSERT_TRUE(info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Load the profile and make sure we can read the data and it matches what we expect.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  run_test(loaded_info);
}

TEST_F(ProfileCompilationInfoTest, AllMethodFlagsOnOneMethod) {
  ProfileCompilationInfo info(/*for_boot_image*/ true);

  // Set all flags on a single method.
  for (uint32_t index = 0; index <= kMaxHotnessFlagBootIndex; index++) {
    AddMethod(&info, dex1, 0, static_cast<Hotness::Flag>(1 << index));
  }

  auto run_test = [&dex1 = dex1](const ProfileCompilationInfo& info) {
    for (uint32_t index = 0; index <= kMaxHotnessFlagBootIndex; index++) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 0)).IsInProfile());
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 0))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index)));
    }
  };
  run_test(info);

  // Save the profile.
  ScratchFile profile;
  ASSERT_TRUE(info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Load the profile and make sure we can read the data and it matches what we expect.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  run_test(loaded_info);
}


TEST_F(ProfileCompilationInfoTest, MethodFlagsMerge) {
  ProfileCompilationInfo info1(/*for_boot_image*/ true);
  ProfileCompilationInfo info2(/*for_boot_image*/ true);

  // Set a few flags on a 2 different methods in each of the profile.
  for (uint32_t index = 0; index <= kMaxHotnessFlagBootIndex / 4; index++) {
    AddMethod(&info1, dex1, 0, static_cast<Hotness::Flag>(1 << index));
    AddMethod(&info2, dex1, 1, static_cast<Hotness::Flag>(1 << index));
  }

  // Set a few more flags on the method 1.
  for (uint32_t index = kMaxHotnessFlagBootIndex / 4 + 1;
       index <= kMaxHotnessFlagBootIndex / 2;
       index++) {
    AddMethod(&info2, dex1, 1, static_cast<Hotness::Flag>(1 << index));
  }

  ASSERT_TRUE(info1.MergeWith(info2));

  auto run_test = [&dex1 = dex1](const ProfileCompilationInfo& info) {
    // Assert that the flags were merged correctly for both methods.
    for (uint32_t index = 0; index <= kMaxHotnessFlagBootIndex / 4; index++) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 0)).IsInProfile());
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 0))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index)));

      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 1)).IsInProfile());
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 1))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index)));
    }

    // Assert that no flags were merged unnecessary.
    for (uint32_t index = kMaxHotnessFlagBootIndex / 4 + 1;
         index <= kMaxHotnessFlagBootIndex / 2;
         index++) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 0)).IsInProfile());
      EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, 0))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index)));

      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 1)).IsInProfile());
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, 1))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index)));
    }

    // Assert that no extra flags were added.
    for (uint32_t index = kMaxHotnessFlagBootIndex / 2 + 1;
         index <= kMaxHotnessFlagBootIndex;
         index++) {
      EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, 0))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index)));
      EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, 1))
          .HasFlagSet(static_cast<Hotness::Flag>(1 << index)));
    }
  };

  run_test(info1);

  // Save the profile.
  ScratchFile profile;
  ASSERT_TRUE(info1.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Load the profile and make sure we can read the data and it matches what we expect.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  run_test(loaded_info);
}

TEST_F(ProfileCompilationInfoTest, SizeStressTestAllIn) {
  SizeStressTest(/*random=*/ false);
}

TEST_F(ProfileCompilationInfoTest, SizeStressTestAllInRandom) {
  SizeStressTest(/*random=*/ true);
}

// Verifies that we correctly add methods to the profile according to their flags.
TEST_F(ProfileCompilationInfoTest, AddMethodsProfileMethodInfoBasic) {
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));

  ProfileCompilationInfo info;

  MethodReference hot(dex.get(), 0);
  MethodReference hot_startup(dex.get(), 1);
  MethodReference startup(dex.get(), 2);

  // Add methods
  ASSERT_TRUE(info.AddMethod(ProfileMethodInfo(hot), Hotness::kFlagHot));
  ASSERT_TRUE(info.AddMethod(
      ProfileMethodInfo(hot_startup),
      static_cast<Hotness::Flag>(Hotness::kFlagHot | Hotness::kFlagStartup)));
  ASSERT_TRUE(info.AddMethod(ProfileMethodInfo(startup), Hotness::kFlagStartup));

  // Verify the profile recorded them correctly.
  EXPECT_TRUE(info.GetMethodHotness(hot).IsInProfile());
  EXPECT_EQ(info.GetMethodHotness(hot).GetFlags(), Hotness::kFlagHot);

  EXPECT_TRUE(info.GetMethodHotness(hot_startup).IsInProfile());
  EXPECT_EQ(info.GetMethodHotness(hot_startup).GetFlags(),
            static_cast<uint32_t>(Hotness::kFlagHot | Hotness::kFlagStartup));

  EXPECT_TRUE(info.GetMethodHotness(startup).IsInProfile());
  EXPECT_EQ(info.GetMethodHotness(startup).GetFlags(), Hotness::kFlagStartup);
}

// Verifies that we correctly add inline caches to the profile only for hot methods.
TEST_F(ProfileCompilationInfoTest, AddMethodsProfileMethodInfoInlineCaches) {
  ProfileCompilationInfo info;
  MethodReference hot(dex1, 0);
  MethodReference startup(dex1, 2);

  // Add inline caches with the methods. The profile should record only the one for the hot method.
  std::vector<TypeReference> types = {};
  ProfileMethodInfo::ProfileInlineCache ic(/*dex_pc*/ 0, /*missing_types*/true, types);
  std::vector<ProfileMethodInfo::ProfileInlineCache> inline_caches = {ic};
  info.AddMethod(ProfileMethodInfo(hot, inline_caches), Hotness::kFlagHot);
  info.AddMethod(ProfileMethodInfo(startup, inline_caches), Hotness::kFlagStartup);

  // Check the hot method's inline cache.
  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> hot_pmi =
      GetMethod(info, dex1, hot.index);
  ASSERT_TRUE(hot_pmi != nullptr);
  ASSERT_EQ(hot_pmi->inline_caches->size(), 1u);
  ASSERT_TRUE(hot_pmi->inline_caches->Get(0).is_missing_types);

  // Check there's no inline caches for the startup method.
  ASSERT_TRUE(GetMethod(info, dex1, startup.index) == nullptr);
}

// Verifies that we correctly add methods to the profile according to their flags.
TEST_F(ProfileCompilationInfoTest, AddMethodsProfileMethodInfoFail) {
  ProfileCompilationInfo info;

  MethodReference hot(dex1, 0);
  MethodReference bad_ref(dex1, kMaxMethodIds);

  std::vector<ProfileMethodInfo> pmis = {ProfileMethodInfo(hot), ProfileMethodInfo(bad_ref)};
  ASSERT_FALSE(info.AddMethods(pmis, Hotness::kFlagHot));
}

// Verify that we can add methods with annotations.
TEST_F(ProfileCompilationInfoTest, AddAnnotationsToMethods) {
  ProfileCompilationInfo info;

  ProfileSampleAnnotation psa1("test1");
  ProfileSampleAnnotation psa2("test2");
  // Save a few methods using different annotations, some overlapping, some not.
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa1));
  }
  for (uint16_t i = 5; i < 15; i++) {
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa2));
  }

  auto run_test = [&dex1 = dex1, &psa1 = psa1, &psa2 = psa2](const ProfileCompilationInfo& info) {
    // Check that all methods are in.
    for (uint16_t i = 0; i < 10; i++) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, i), psa1).IsInProfile());
      EXPECT_TRUE(info.GetHotMethodInfo(MethodReference(dex1, i), psa1) != nullptr);
    }
    for (uint16_t i = 5; i < 15; i++) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, i), psa2).IsInProfile());
      EXPECT_TRUE(info.GetHotMethodInfo(MethodReference(dex1, i), psa2) != nullptr);
    }
    // Check that the non-overlapping methods are not added with a wrong annotation.
    for (uint16_t i = 10; i < 15; i++) {
      EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, i), psa1).IsInProfile());
      EXPECT_FALSE(info.GetHotMethodInfo(MethodReference(dex1, i), psa1) != nullptr);
    }
    for (uint16_t i = 0; i < 5; i++) {
      EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, i), psa2).IsInProfile());
      EXPECT_FALSE(info.GetHotMethodInfo(MethodReference(dex1, i), psa2) != nullptr);
    }
    // Check that when querying without an annotation only the first one is searched.
    for (uint16_t i = 0; i < 10; i++) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, i)).IsInProfile());
      EXPECT_TRUE(info.GetHotMethodInfo(MethodReference(dex1, i)) != nullptr);
    }
    // ... this should be false because they belong the second appearance of dex1.
    for (uint16_t i = 10; i < 15; i++) {
      EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, i)).IsInProfile());
      EXPECT_FALSE(info.GetHotMethodInfo(MethodReference(dex1, i)) != nullptr);
    }

    // Sanity check that methods cannot be found with a non existing annotation.
    MethodReference ref(dex1, 0);
    ProfileSampleAnnotation not_exisiting("A");
    EXPECT_FALSE(info.GetMethodHotness(ref, not_exisiting).IsInProfile());
    EXPECT_FALSE(info.GetHotMethodInfo(ref, not_exisiting) != nullptr);
  };

  // Run the test before save.
  run_test(info);

  ScratchFile profile;
  ASSERT_TRUE(info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(info));

  // Run the test after save and load.
  run_test(loaded_info);
}

// Verify that we can add classes with annotations.
TEST_F(ProfileCompilationInfoTest, AddAnnotationsToClasses) {
  ProfileCompilationInfo info;

  ProfileSampleAnnotation psa1("test1");
  ProfileSampleAnnotation psa2("test2");
  // Save a few classes using different annotations, some overlapping, some not.
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddClass(&info, dex1, dex::TypeIndex(i), psa1));
  }
  for (uint16_t i = 5; i < 15; i++) {
    ASSERT_TRUE(AddClass(&info, dex1, dex::TypeIndex(i), psa2));
  }

  auto run_test = [&dex1 = dex1, &psa1 = psa1, &psa2 = psa2](const ProfileCompilationInfo& info) {
    // Check that all classes are in.
    for (uint16_t i = 0; i < 10; i++) {
      EXPECT_TRUE(info.ContainsClass(*dex1, dex::TypeIndex(i), psa1));
    }
    for (uint16_t i = 5; i < 15; i++) {
      EXPECT_TRUE(info.ContainsClass(*dex1, dex::TypeIndex(i), psa2));
    }
    // Check that the non-overlapping classes are not added with a wrong annotation.
    for (uint16_t i = 10; i < 15; i++) {
      EXPECT_FALSE(info.ContainsClass(*dex1, dex::TypeIndex(i), psa1));
    }
    for (uint16_t i = 0; i < 5; i++) {
      EXPECT_FALSE(info.ContainsClass(*dex1, dex::TypeIndex(i), psa2));
    }
    // Check that when querying without an annotation only the first one is searched.
    for (uint16_t i = 0; i < 10; i++) {
      EXPECT_TRUE(info.ContainsClass(*dex1, dex::TypeIndex(i)));
    }
    // ... this should be false because they belong the second appearance of dex1.
    for (uint16_t i = 10; i < 15; i++) {
      EXPECT_FALSE(info.ContainsClass(*dex1, dex::TypeIndex(i)));
    }

    // Sanity check that classes cannot be found with a non existing annotation.
    EXPECT_FALSE(info.ContainsClass(*dex1, dex::TypeIndex(0), ProfileSampleAnnotation("new_test")));
  };

  // Run the test before save.
  run_test(info);

  ScratchFile profile;
  ASSERT_TRUE(info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(info));

  // Run the test after save and load.
  run_test(loaded_info);
}

// Verify we can merge samples with annotations.
TEST_F(ProfileCompilationInfoTest, MergeWithAnnotations) {
  ProfileCompilationInfo info1;
  ProfileCompilationInfo info2;

  ProfileSampleAnnotation psa1("test1");
  ProfileSampleAnnotation psa2("test2");

  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod(&info1, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa1));
    ASSERT_TRUE(AddClass(&info1, dex1, dex::TypeIndex(i), psa1));
  }
  for (uint16_t i = 5; i < 15; i++) {
    ASSERT_TRUE(AddMethod(&info2, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa1));
    ASSERT_TRUE(AddMethod(&info2, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa2));
    ASSERT_TRUE(AddMethod(&info2, dex2, /* method_idx= */ i, Hotness::kFlagHot, psa2));
    ASSERT_TRUE(AddClass(&info2, dex1, dex::TypeIndex(i), psa1));
    ASSERT_TRUE(AddClass(&info2, dex1, dex::TypeIndex(i), psa2));
  }

  ProfileCompilationInfo info;
  ASSERT_TRUE(info.MergeWith(info1));
  ASSERT_TRUE(info.MergeWith(info2));

  // Check that all items are in.
  for (uint16_t i = 0; i < 15; i++) {
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, i), psa1).IsInProfile());
    EXPECT_TRUE(info.ContainsClass(*dex1, dex::TypeIndex(i), psa1));
  }
  for (uint16_t i = 5; i < 15; i++) {
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex1, i), psa2).IsInProfile());
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex2, i), psa2).IsInProfile());
    EXPECT_TRUE(info.ContainsClass(*dex1, dex::TypeIndex(i), psa2));
  }

  // Check that the non-overlapping items are not added with a wrong annotation.
  for (uint16_t i = 0; i < 5; i++) {
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex1, i), psa2).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex2, i), psa2).IsInProfile());
    EXPECT_FALSE(info.ContainsClass(*dex1, dex::TypeIndex(i), psa2));
  }
}

// Verify the bulk extraction API.
TEST_F(ProfileCompilationInfoTest, ExtractInfoWithAnnations) {
  ProfileCompilationInfo info;

  ProfileSampleAnnotation psa1("test1");
  ProfileSampleAnnotation psa2("test2");

  std::set<dex::TypeIndex> expected_classes;
  std::set<uint16_t> expected_hot_methods;
  std::set<uint16_t> expected_startup_methods;
  std::set<uint16_t> expected_post_startup_methods;

  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa1));
    ASSERT_TRUE(AddClass(&info, dex1, dex::TypeIndex(i), psa1));
    expected_hot_methods.insert(i);
    expected_classes.insert(dex::TypeIndex(i));
  }
  for (uint16_t i = 5; i < 15; i++) {
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa2));
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i, Hotness::kFlagStartup, psa1));
    expected_startup_methods.insert(i);
  }

  std::set<dex::TypeIndex> classes;
  std::set<uint16_t> hot_methods;
  std::set<uint16_t> startup_methods;
  std::set<uint16_t> post_startup_methods;

  EXPECT_TRUE(info.GetClassesAndMethods(
      *dex1, &classes, &hot_methods, &startup_methods, &post_startup_methods, psa1));
  EXPECT_EQ(expected_classes, classes);
  EXPECT_EQ(expected_hot_methods, hot_methods);
  EXPECT_EQ(expected_startup_methods, startup_methods);
  EXPECT_EQ(expected_post_startup_methods, post_startup_methods);

  EXPECT_FALSE(info.GetClassesAndMethods(
      *dex1,
      &classes,
      &hot_methods,
      &startup_methods,
      &post_startup_methods,
      ProfileSampleAnnotation("new_test")));
}

// Verify the behavior for adding methods with annotations and different dex checksums.
TEST_F(ProfileCompilationInfoTest, AddMethodsWithAnnotationAndDifferentChecksum) {
  ProfileCompilationInfo info;

  ProfileSampleAnnotation psa1("test1");
  ProfileSampleAnnotation psa2("test2");

  MethodReference ref(dex1, 0);
  MethodReference ref_checksum_missmatch(dex1_checksum_missmatch, 1);

  ASSERT_TRUE(info.AddMethod(ProfileMethodInfo(ref), Hotness::kFlagHot, psa1));
  // Adding a method with a different dex checksum and the same annotation should fail.
  ASSERT_FALSE(info.AddMethod(ProfileMethodInfo(ref_checksum_missmatch), Hotness::kFlagHot, psa1));
  // However, a method with a different dex checksum and a different annotation should be ok.
  ASSERT_TRUE(info.AddMethod(ProfileMethodInfo(ref_checksum_missmatch), Hotness::kFlagHot, psa2));
}

// Verify the behavior for searching method with annotations and different dex checksums.
TEST_F(ProfileCompilationInfoTest, FindMethodsWithAnnotationAndDifferentChecksum) {
  ProfileCompilationInfo info;

  ProfileSampleAnnotation psa1("test1");

  MethodReference ref(dex1, 0);
  MethodReference ref_checksum_missmatch(dex1_checksum_missmatch, 0);

  ASSERT_TRUE(info.AddMethod(ProfileMethodInfo(ref), Hotness::kFlagHot, psa1));

  // The method should be in the profile when searched with the correct data.
  EXPECT_TRUE(info.GetMethodHotness(ref, psa1).IsInProfile());
  // We should get a negative result if the dex checksum  does not match.
  EXPECT_FALSE(info.GetMethodHotness(ref_checksum_missmatch, psa1).IsInProfile());

  // If we search without annotation we should have the same behaviour.
  EXPECT_TRUE(info.GetMethodHotness(ref).IsInProfile());
  EXPECT_FALSE(info.GetMethodHotness(ref_checksum_missmatch).IsInProfile());
}

TEST_F(ProfileCompilationInfoTest, ClearDataAndAdjustVersionRegularToBoot) {
  ProfileCompilationInfo info;

  AddMethod(&info, dex1, /* method_idx= */ 0, Hotness::kFlagHot);

  info.ClearDataAndAdjustVersion(/*for_boot_image=*/true);
  ASSERT_TRUE(info.IsEmpty());
  ASSERT_TRUE(info.IsForBootImage());
}

TEST_F(ProfileCompilationInfoTest, ClearDataAndAdjustVersionBootToRegular) {
  ProfileCompilationInfo info(/*for_boot_image=*/true);

  AddMethod(&info, dex1, /* method_idx= */ 0, Hotness::kFlagHot);

  info.ClearDataAndAdjustVersion(/*for_boot_image=*/false);
  ASSERT_TRUE(info.IsEmpty());
  ASSERT_FALSE(info.IsForBootImage());
}

template<class T>
static std::list<T> sort(const std::list<T>& list) {
  std::list<T> copy(list);
  copy.sort();
  return copy;
}

// Verify we can extract profile data
TEST_F(ProfileCompilationInfoTest, ExtractProfileData) {
  // Setup test data
  ProfileCompilationInfo info;

  ProfileSampleAnnotation psa1("test1");
  ProfileSampleAnnotation psa2("test2");

  for (uint16_t i = 0; i < 10; i++) {
    // Add dex1 data with different annotations so that we can check the annotation count.
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa1));
    ASSERT_TRUE(AddClass(&info, dex1, dex::TypeIndex(i), psa1));
    ASSERT_TRUE(AddMethod(&info, dex1, /* method_idx= */ i, Hotness::kFlagStartup, psa2));
    ASSERT_TRUE(AddClass(&info, dex1, dex::TypeIndex(i), psa2));
    ASSERT_TRUE(AddMethod(&info, dex2, /* method_idx= */ i, Hotness::kFlagHot, psa2));
    // dex3 will not be used in the data extraction
    ASSERT_TRUE(AddMethod(&info, dex3, /* method_idx= */ i, Hotness::kFlagHot, psa2));
  }

  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files.push_back(std::unique_ptr<const DexFile>(dex1));
  dex_files.push_back(std::unique_ptr<const DexFile>(dex2));

  // Run the test: extract the data for dex1 and dex2
  std::unique_ptr<FlattenProfileData> flattenProfileData = info.ExtractProfileData(dex_files);

  // Check the results
  ASSERT_TRUE(flattenProfileData != nullptr);
  ASSERT_EQ(flattenProfileData->GetMaxAggregationForMethods(), 2u);
  ASSERT_EQ(flattenProfileData->GetMaxAggregationForClasses(), 2u);

  const SafeMap<MethodReference, ItemMetadata>& methods = flattenProfileData->GetMethodData();
  const SafeMap<TypeReference, ItemMetadata>& classes = flattenProfileData->GetClassData();
  ASSERT_EQ(methods.size(), 20u);  // 10 methods in dex1, 10 in dex2
  ASSERT_EQ(classes.size(), 10u);  // 10 methods in dex1

  std::list<ProfileSampleAnnotation> expectedAnnotations1({psa1, psa2});
  std::list<ProfileSampleAnnotation> expectedAnnotations2({psa2});
  for (uint16_t i = 0; i < 10; i++) {
    // Check dex1 methods.
    auto mIt1 = methods.find(MethodReference(dex1, i));
    ASSERT_TRUE(mIt1 != methods.end());
    ASSERT_EQ(mIt1->second.GetFlags(), Hotness::kFlagHot | Hotness::kFlagStartup);
    ASSERT_EQ(sort(mIt1->second.GetAnnotations()), expectedAnnotations1);
    // Check dex1 classes
    auto cIt1 = classes.find(TypeReference(dex1, dex::TypeIndex(i)));
    ASSERT_TRUE(cIt1 != classes.end());
    ASSERT_EQ(cIt1->second.GetFlags(), 0);
    ASSERT_EQ(sort(cIt1->second.GetAnnotations()), expectedAnnotations1);
    // Check dex2 methods.
    auto mIt2 = methods.find(MethodReference(dex2, i));
    ASSERT_TRUE(mIt2 != methods.end());
    ASSERT_EQ(mIt2->second.GetFlags(), Hotness::kFlagHot);
    ASSERT_EQ(sort(mIt2->second.GetAnnotations()), expectedAnnotations2);
  }

  // Release the ownership as this is held by the test class;
  for (std::unique_ptr<const DexFile>& dex : dex_files) {
    UNUSED(dex.release());
  }
}

// Verify we can merge 2 previously flatten data.
TEST_F(ProfileCompilationInfoTest, MergeFlattenData) {
  // Setup test data: two profiles with different content which will be used
  // to extract FlattenProfileData, later to be merged.
  ProfileCompilationInfo info1;
  ProfileCompilationInfo info2;

  ProfileSampleAnnotation psa1("test1");
  ProfileSampleAnnotation psa2("test2");

  for (uint16_t i = 0; i < 10; i++) {
    // Add dex1 data with different annotations so that we can check the annotation count.
    ASSERT_TRUE(AddMethod(&info1, dex1, /* method_idx= */ i, Hotness::kFlagHot, psa1));
    ASSERT_TRUE(AddClass(&info2, dex1, dex::TypeIndex(i), psa1));
    ASSERT_TRUE(AddMethod(&info1, dex1, /* method_idx= */ i, Hotness::kFlagStartup, psa2));
    ASSERT_TRUE(AddClass(&info1, dex1, dex::TypeIndex(i), psa2));
    ASSERT_TRUE(AddMethod(i % 2 == 0 ? &info1 : &info2, dex2,
                          /* method_idx= */ i,
                          Hotness::kFlagHot,
                          psa2));
  }

  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files.push_back(std::unique_ptr<const DexFile>(dex1));
  dex_files.push_back(std::unique_ptr<const DexFile>(dex2));

  // Run the test: extract the data for dex1 and dex2 and then merge it into
  std::unique_ptr<FlattenProfileData> flattenProfileData1 = info1.ExtractProfileData(dex_files);
  std::unique_ptr<FlattenProfileData> flattenProfileData2 = info2.ExtractProfileData(dex_files);

  flattenProfileData1->MergeData(*flattenProfileData2);
  // Check the results
  ASSERT_EQ(flattenProfileData1->GetMaxAggregationForMethods(), 2u);
  ASSERT_EQ(flattenProfileData1->GetMaxAggregationForClasses(), 2u);

  const SafeMap<MethodReference, ItemMetadata>& methods = flattenProfileData1->GetMethodData();
  const SafeMap<TypeReference, ItemMetadata>& classes = flattenProfileData1->GetClassData();
  ASSERT_EQ(methods.size(), 20u);  // 10 methods in dex1, 10 in dex2
  ASSERT_EQ(classes.size(), 10u);  // 10 methods in dex1

  std::list<ProfileSampleAnnotation> expectedAnnotations1({psa1, psa2});
  std::list<ProfileSampleAnnotation> expectedAnnotations2({psa2});
  for (uint16_t i = 0; i < 10; i++) {
    // Check dex1 methods.
    auto mIt1 = methods.find(MethodReference(dex1, i));
    ASSERT_TRUE(mIt1 != methods.end());
    ASSERT_EQ(mIt1->second.GetFlags(), Hotness::kFlagHot | Hotness::kFlagStartup);
    ASSERT_EQ(sort(mIt1->second.GetAnnotations()), expectedAnnotations1);
    // Check dex1 classes
    auto cIt1 = classes.find(TypeReference(dex1, dex::TypeIndex(i)));
    ASSERT_TRUE(cIt1 != classes.end());
    ASSERT_EQ(cIt1->second.GetFlags(), 0);
    ASSERT_EQ(sort(cIt1->second.GetAnnotations()).size(), expectedAnnotations1.size());
    ASSERT_EQ(sort(cIt1->second.GetAnnotations()), expectedAnnotations1);
    // Check dex2 methods.
    auto mIt2 = methods.find(MethodReference(dex2, i));
    ASSERT_TRUE(mIt2 != methods.end());
    ASSERT_EQ(mIt2->second.GetFlags(), Hotness::kFlagHot);
    ASSERT_EQ(sort(mIt2->second.GetAnnotations()), expectedAnnotations2);
  }

  // Release the ownership as this is held by the test class;
  for (std::unique_ptr<const DexFile>& dex : dex_files) {
    UNUSED(dex.release());
  }
}

}  // namespace art
