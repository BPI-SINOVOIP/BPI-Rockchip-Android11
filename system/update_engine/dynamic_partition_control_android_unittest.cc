//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/dynamic_partition_control_android.h"

#include <set>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libavb/libavb.h>

#include "update_engine/common/mock_prefs.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/dynamic_partition_test_utils.h"
#include "update_engine/mock_dynamic_partition_control.h"

using android::dm::DmDeviceState;
using chromeos_update_engine::test_utils::ScopedLoopbackDeviceBinder;
using chromeos_update_engine::test_utils::ScopedTempFile;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::AnyOf;
using testing::Invoke;
using testing::NiceMock;
using testing::Not;
using testing::Optional;
using testing::Return;

namespace chromeos_update_engine {

class DynamicPartitionControlAndroidTest : public ::testing::Test {
 public:
  void SetUp() override {
    module_ = std::make_unique<NiceMock<MockDynamicPartitionControlAndroid>>();

    ON_CALL(dynamicControl(), GetDynamicPartitionsFeatureFlag())
        .WillByDefault(Return(FeatureFlag(FeatureFlag::Value::LAUNCH)));
    ON_CALL(dynamicControl(), GetVirtualAbFeatureFlag())
        .WillByDefault(Return(FeatureFlag(FeatureFlag::Value::NONE)));

    ON_CALL(dynamicControl(), GetDeviceDir(_))
        .WillByDefault(Invoke([](auto path) {
          *path = kFakeDevicePath;
          return true;
        }));

    ON_CALL(dynamicControl(), GetSuperPartitionName(_))
        .WillByDefault(Return(kFakeSuper));

    ON_CALL(dynamicControl(), GetDmDevicePathByName(_, _))
        .WillByDefault(Invoke([](auto partition_name_suffix, auto device) {
          *device = GetDmDevice(partition_name_suffix);
          return true;
        }));

    ON_CALL(dynamicControl(), EraseSystemOtherAvbFooter(_, _))
        .WillByDefault(Return(true));
  }

  // Return the mocked DynamicPartitionControlInterface.
  NiceMock<MockDynamicPartitionControlAndroid>& dynamicControl() {
    return static_cast<NiceMock<MockDynamicPartitionControlAndroid>&>(*module_);
  }

  std::string GetSuperDevice(uint32_t slot) {
    return GetDevice(dynamicControl().GetSuperPartitionName(slot));
  }

  uint32_t source() { return slots_.source; }
  uint32_t target() { return slots_.target; }

  // Return partition names with suffix of source().
  std::string S(const std::string& name) {
    return name + kSlotSuffixes[source()];
  }

  // Return partition names with suffix of target().
  std::string T(const std::string& name) {
    return name + kSlotSuffixes[target()];
  }

  // Set the fake metadata to return when LoadMetadataBuilder is called on
  // |slot|.
  void SetMetadata(uint32_t slot,
                   const PartitionSuffixSizes& sizes,
                   uint32_t partition_attr = 0) {
    EXPECT_CALL(dynamicControl(),
                LoadMetadataBuilder(GetSuperDevice(slot), slot, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([sizes, partition_attr](auto, auto, auto) {
          return NewFakeMetadata(PartitionSuffixSizesToManifest(sizes),
                                 partition_attr);
        }));
  }

  void ExpectStoreMetadata(const PartitionSuffixSizes& partition_sizes) {
    EXPECT_CALL(dynamicControl(),
                StoreMetadata(GetSuperDevice(target()),
                              MetadataMatches(partition_sizes),
                              target()))
        .WillOnce(Return(true));
  }

  // Expect that UnmapPartitionOnDeviceMapper is called on target() metadata
  // slot with each partition in |partitions|.
  void ExpectUnmap(const std::set<std::string>& partitions) {
    // Error when UnmapPartitionOnDeviceMapper is called on unknown arguments.
    ON_CALL(dynamicControl(), UnmapPartitionOnDeviceMapper(_))
        .WillByDefault(Return(false));

    for (const auto& partition : partitions) {
      EXPECT_CALL(dynamicControl(), UnmapPartitionOnDeviceMapper(partition))
          .WillOnce(Return(true));
    }
  }
  bool PreparePartitionsForUpdate(const PartitionSizes& partition_sizes) {
    return dynamicControl().PreparePartitionsForUpdate(
        source(),
        target(),
        PartitionSizesToManifest(partition_sizes),
        true,
        nullptr);
  }
  void SetSlots(const TestParam& slots) { slots_ = slots; }

  void SetSnapshotEnabled(bool enabled) {
    dynamicControl().target_supports_snapshot_ = enabled;
  }

  struct Listener : public ::testing::MatchResultListener {
    explicit Listener(std::ostream* os) : MatchResultListener(os) {}
  };

  testing::AssertionResult UpdatePartitionMetadata(
      const PartitionSuffixSizes& source_metadata,
      const PartitionSizes& update_metadata,
      const PartitionSuffixSizes& expected) {
    return UpdatePartitionMetadata(
        PartitionSuffixSizesToManifest(source_metadata),
        PartitionSizesToManifest(update_metadata),
        PartitionSuffixSizesToManifest(expected));
  }
  testing::AssertionResult UpdatePartitionMetadata(
      const DeltaArchiveManifest& source_manifest,
      const DeltaArchiveManifest& update_manifest,
      const DeltaArchiveManifest& expected) {
    return UpdatePartitionMetadata(
        source_manifest, update_manifest, MetadataMatches(expected));
  }
  testing::AssertionResult UpdatePartitionMetadata(
      const DeltaArchiveManifest& source_manifest,
      const DeltaArchiveManifest& update_manifest,
      const Matcher<MetadataBuilder*>& matcher) {
    auto super_metadata = NewFakeMetadata(source_manifest);
    if (!module_->UpdatePartitionMetadata(
            super_metadata.get(), target(), update_manifest)) {
      return testing::AssertionFailure()
             << "UpdatePartitionMetadataInternal failed";
    }
    std::stringstream ss;
    Listener listener(&ss);
    if (matcher.MatchAndExplain(super_metadata.get(), &listener)) {
      return testing::AssertionSuccess() << ss.str();
    } else {
      return testing::AssertionFailure() << ss.str();
    }
  }

  std::unique_ptr<DynamicPartitionControlAndroid> module_;
  TestParam slots_;
};

class DynamicPartitionControlAndroidTestP
    : public DynamicPartitionControlAndroidTest,
      public ::testing::WithParamInterface<TestParam> {
 public:
  void SetUp() override {
    DynamicPartitionControlAndroidTest::SetUp();
    SetSlots(GetParam());
  }
};

// Test resize case. Grow if target metadata contains a partition with a size
// less than expected.
TEST_P(DynamicPartitionControlAndroidTestP,
       NeedGrowIfSizeNotMatchWhenResizing) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  PartitionSuffixSizes expected{{S("system"), 2_GiB},
                                {S("vendor"), 1_GiB},
                                {T("system"), 3_GiB},
                                {T("vendor"), 1_GiB}};
  PartitionSizes update_metadata{{"system", 3_GiB}, {"vendor", 1_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test resize case. Shrink if target metadata contains a partition with a size
// greater than expected.
TEST_P(DynamicPartitionControlAndroidTestP,
       NeedShrinkIfSizeNotMatchWhenResizing) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  PartitionSuffixSizes expected{{S("system"), 2_GiB},
                                {S("vendor"), 1_GiB},
                                {T("system"), 2_GiB},
                                {T("vendor"), 150_MiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}, {"vendor", 150_MiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test adding partitions on the first run.
TEST_P(DynamicPartitionControlAndroidTestP, AddPartitionToEmptyMetadata) {
  PartitionSuffixSizes source_metadata{};
  PartitionSuffixSizes expected{{T("system"), 2_GiB}, {T("vendor"), 1_GiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}, {"vendor", 1_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test subsequent add case.
TEST_P(DynamicPartitionControlAndroidTestP, AddAdditionalPartition) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {T("system"), 2_GiB}};
  PartitionSuffixSizes expected{
      {S("system"), 2_GiB}, {T("system"), 2_GiB}, {T("vendor"), 1_GiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}, {"vendor", 1_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test delete one partition.
TEST_P(DynamicPartitionControlAndroidTestP, DeletePartition) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  // No T("vendor")
  PartitionSuffixSizes expected{
      {S("system"), 2_GiB}, {S("vendor"), 1_GiB}, {T("system"), 2_GiB}};
  PartitionSizes update_metadata{{"system", 2_GiB}};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test delete all partitions.
TEST_P(DynamicPartitionControlAndroidTestP, DeleteAll) {
  PartitionSuffixSizes source_metadata{{S("system"), 2_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 2_GiB},
                                       {T("vendor"), 1_GiB}};
  PartitionSuffixSizes expected{{S("system"), 2_GiB}, {S("vendor"), 1_GiB}};
  PartitionSizes update_metadata{};
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_metadata, update_metadata, expected));
}

// Test corrupt source metadata case.
TEST_P(DynamicPartitionControlAndroidTestP, CorruptedSourceMetadata) {
  EXPECT_CALL(dynamicControl(),
              LoadMetadataBuilder(GetSuperDevice(source()), source(), _))
      .WillOnce(Invoke([](auto, auto, auto) { return nullptr; }));
  ExpectUnmap({T("system")});

  EXPECT_FALSE(PreparePartitionsForUpdate({{"system", 1_GiB}}))
      << "Should not be able to continue with corrupt source metadata";
}

// Test that UpdatePartitionMetadata fails if there is not enough space on the
// device.
TEST_P(DynamicPartitionControlAndroidTestP, NotEnoughSpace) {
  PartitionSuffixSizes source_metadata{{S("system"), 3_GiB},
                                       {S("vendor"), 2_GiB},
                                       {T("system"), 0},
                                       {T("vendor"), 0}};
  PartitionSizes update_metadata{{"system", 3_GiB}, {"vendor", 3_GiB}};

  EXPECT_FALSE(UpdatePartitionMetadata(source_metadata, update_metadata, {}))
      << "Should not be able to fit 11GiB data into 10GiB space";
}

TEST_P(DynamicPartitionControlAndroidTestP, NotEnoughSpaceForSlot) {
  PartitionSuffixSizes source_metadata{{S("system"), 1_GiB},
                                       {S("vendor"), 1_GiB},
                                       {T("system"), 0},
                                       {T("vendor"), 0}};
  PartitionSizes update_metadata{{"system", 3_GiB}, {"vendor", 3_GiB}};
  EXPECT_FALSE(UpdatePartitionMetadata(source_metadata, update_metadata, {}))
      << "Should not be able to grow over size of super / 2";
}

TEST_P(DynamicPartitionControlAndroidTestP,
       ApplyRetrofitUpdateOnDynamicPartitionsEnabledBuild) {
  ON_CALL(dynamicControl(), GetDynamicPartitionsFeatureFlag())
      .WillByDefault(Return(FeatureFlag(FeatureFlag::Value::RETROFIT)));
  // Static partition {system,bar}_{a,b} exists.
  EXPECT_CALL(dynamicControl(),
              DeviceExists(AnyOf(GetDevice(S("bar")),
                                 GetDevice(T("bar")),
                                 GetDevice(S("system")),
                                 GetDevice(T("system")))))
      .WillRepeatedly(Return(true));

  SetMetadata(source(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 1_GiB}});

  // Not calling through
  // DynamicPartitionControlAndroidTest::PreparePartitionsForUpdate(), since we
  // don't want any default group in the PartitionMetadata.
  EXPECT_TRUE(dynamicControl().PreparePartitionsForUpdate(
      source(), target(), {}, true, nullptr));

  // Should use dynamic source partitions.
  EXPECT_CALL(dynamicControl(), GetState(S("system")))
      .Times(1)
      .WillOnce(Return(DmDeviceState::ACTIVE));
  string system_device;
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "system", source(), source(), &system_device));
  EXPECT_EQ(GetDmDevice(S("system")), system_device);

  // Should use static target partitions without querying dynamic control.
  EXPECT_CALL(dynamicControl(), GetState(T("system"))).Times(0);
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "system", target(), source(), &system_device));
  EXPECT_EQ(GetDevice(T("system")), system_device);

  // Static partition "bar".
  EXPECT_CALL(dynamicControl(), GetState(S("bar"))).Times(0);
  std::string bar_device;
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "bar", source(), source(), &bar_device));
  EXPECT_EQ(GetDevice(S("bar")), bar_device);

  EXPECT_CALL(dynamicControl(), GetState(T("bar"))).Times(0);
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "bar", target(), source(), &bar_device));
  EXPECT_EQ(GetDevice(T("bar")), bar_device);
}

TEST_P(DynamicPartitionControlAndroidTestP,
       GetPartitionDeviceWhenResumingUpdate) {
  // Static partition bar_{a,b} exists.
  EXPECT_CALL(dynamicControl(),
              DeviceExists(AnyOf(GetDevice(S("bar")), GetDevice(T("bar")))))
      .WillRepeatedly(Return(true));

  // Both of the two slots contain valid partition metadata, since this is
  // resuming an update.
  SetMetadata(source(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 1_GiB}});
  SetMetadata(target(),
              {{S("system"), 2_GiB},
               {S("vendor"), 1_GiB},
               {T("system"), 2_GiB},
               {T("vendor"), 1_GiB}});

  EXPECT_TRUE(dynamicControl().PreparePartitionsForUpdate(
      source(),
      target(),
      PartitionSizesToManifest({{"system", 2_GiB}, {"vendor", 1_GiB}}),
      false,
      nullptr));

  // Dynamic partition "system".
  EXPECT_CALL(dynamicControl(), GetState(S("system")))
      .Times(1)
      .WillOnce(Return(DmDeviceState::ACTIVE));
  string system_device;
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "system", source(), source(), &system_device));
  EXPECT_EQ(GetDmDevice(S("system")), system_device);

  EXPECT_CALL(dynamicControl(), GetState(T("system")))
      .Times(AnyNumber())
      .WillOnce(Return(DmDeviceState::ACTIVE));
  EXPECT_CALL(dynamicControl(),
              MapPartitionOnDeviceMapper(
                  GetSuperDevice(target()), T("system"), target(), _, _))
      .Times(AnyNumber())
      .WillRepeatedly(
          Invoke([](const auto&, const auto& name, auto, auto, auto* device) {
            *device = "/fake/remapped/" + name;
            return true;
          }));
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "system", target(), source(), &system_device));
  EXPECT_EQ("/fake/remapped/" + T("system"), system_device);

  // Static partition "bar".
  EXPECT_CALL(dynamicControl(), GetState(S("bar"))).Times(0);
  std::string bar_device;
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "bar", source(), source(), &bar_device));
  EXPECT_EQ(GetDevice(S("bar")), bar_device);

  EXPECT_CALL(dynamicControl(), GetState(T("bar"))).Times(0);
  EXPECT_TRUE(dynamicControl().GetPartitionDevice(
      "bar", target(), source(), &bar_device));
  EXPECT_EQ(GetDevice(T("bar")), bar_device);
}

INSTANTIATE_TEST_CASE_P(DynamicPartitionControlAndroidTest,
                        DynamicPartitionControlAndroidTestP,
                        testing::Values(TestParam{0, 1}, TestParam{1, 0}));

class DynamicPartitionControlAndroidGroupTestP
    : public DynamicPartitionControlAndroidTestP {
 public:
  DeltaArchiveManifest source_manifest;
  void SetUp() override {
    DynamicPartitionControlAndroidTestP::SetUp();
    AddGroupAndPartition(
        &source_manifest, S("android"), 3_GiB, S("system"), 2_GiB);
    AddGroupAndPartition(&source_manifest, S("oem"), 2_GiB, S("vendor"), 1_GiB);
    AddGroupAndPartition(&source_manifest, T("android"), 3_GiB, T("system"), 0);
    AddGroupAndPartition(&source_manifest, T("oem"), 2_GiB, T("vendor"), 0);
  }

  void AddGroupAndPartition(DeltaArchiveManifest* manifest,
                            const string& group,
                            uint64_t group_size,
                            const string& partition,
                            uint64_t partition_size) {
    auto* g = AddGroup(manifest, group, group_size);
    AddPartition(manifest, g, partition, partition_size);
  }
};

// Allow to resize within group.
TEST_P(DynamicPartitionControlAndroidGroupTestP, ResizeWithinGroup) {
  DeltaArchiveManifest expected;
  AddGroupAndPartition(&expected, T("android"), 3_GiB, T("system"), 3_GiB);
  AddGroupAndPartition(&expected, T("oem"), 2_GiB, T("vendor"), 2_GiB);

  DeltaArchiveManifest update_manifest;
  AddGroupAndPartition(&update_manifest, "android", 3_GiB, "system", 3_GiB);
  AddGroupAndPartition(&update_manifest, "oem", 2_GiB, "vendor", 2_GiB);

  EXPECT_TRUE(
      UpdatePartitionMetadata(source_manifest, update_manifest, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, NotEnoughSpaceForGroup) {
  DeltaArchiveManifest update_manifest;
  AddGroupAndPartition(&update_manifest, "android", 3_GiB, "system", 1_GiB),
      AddGroupAndPartition(&update_manifest, "oem", 2_GiB, "vendor", 3_GiB);
  EXPECT_FALSE(UpdatePartitionMetadata(source_manifest, update_manifest, {}))
      << "Should not be able to grow over maximum size of group";
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, GroupTooBig) {
  DeltaArchiveManifest update_manifest;
  AddGroup(&update_manifest, "android", 3_GiB);
  AddGroup(&update_manifest, "oem", 3_GiB);
  EXPECT_FALSE(UpdatePartitionMetadata(source_manifest, update_manifest, {}))
      << "Should not be able to grow over size of super / 2";
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, AddPartitionToGroup) {
  DeltaArchiveManifest expected;
  auto* g = AddGroup(&expected, T("android"), 3_GiB);
  AddPartition(&expected, g, T("system"), 2_GiB);
  AddPartition(&expected, g, T("system_ext"), 1_GiB);

  DeltaArchiveManifest update_manifest;
  g = AddGroup(&update_manifest, "android", 3_GiB);
  AddPartition(&update_manifest, g, "system", 2_GiB);
  AddPartition(&update_manifest, g, "system_ext", 1_GiB);
  AddGroupAndPartition(&update_manifest, "oem", 2_GiB, "vendor", 2_GiB);

  EXPECT_TRUE(
      UpdatePartitionMetadata(source_manifest, update_manifest, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, RemovePartitionFromGroup) {
  DeltaArchiveManifest expected;
  AddGroup(&expected, T("android"), 3_GiB);

  DeltaArchiveManifest update_manifest;
  AddGroup(&update_manifest, "android", 3_GiB);
  AddGroupAndPartition(&update_manifest, "oem", 2_GiB, "vendor", 2_GiB);

  EXPECT_TRUE(
      UpdatePartitionMetadata(source_manifest, update_manifest, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, AddGroup) {
  DeltaArchiveManifest expected;
  AddGroupAndPartition(
      &expected, T("new_group"), 2_GiB, T("new_partition"), 2_GiB);

  DeltaArchiveManifest update_manifest;
  AddGroupAndPartition(&update_manifest, "android", 2_GiB, "system", 2_GiB);
  AddGroupAndPartition(&update_manifest, "oem", 1_GiB, "vendor", 1_GiB);
  AddGroupAndPartition(
      &update_manifest, "new_group", 2_GiB, "new_partition", 2_GiB);
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_manifest, update_manifest, expected));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, RemoveGroup) {
  DeltaArchiveManifest update_manifest;
  AddGroupAndPartition(&update_manifest, "android", 2_GiB, "system", 2_GiB);

  EXPECT_TRUE(UpdatePartitionMetadata(
      source_manifest, update_manifest, Not(HasGroup(T("oem")))));
}

TEST_P(DynamicPartitionControlAndroidGroupTestP, ResizeGroup) {
  DeltaArchiveManifest expected;
  AddGroupAndPartition(&expected, T("android"), 2_GiB, T("system"), 2_GiB);
  AddGroupAndPartition(&expected, T("oem"), 3_GiB, T("vendor"), 3_GiB);
  DeltaArchiveManifest update_manifest;
  AddGroupAndPartition(&update_manifest, "android", 2_GiB, "system", 2_GiB),
      AddGroupAndPartition(&update_manifest, "oem", 3_GiB, "vendor", 3_GiB);
  EXPECT_TRUE(
      UpdatePartitionMetadata(source_manifest, update_manifest, expected));
}

INSTANTIATE_TEST_CASE_P(DynamicPartitionControlAndroidTest,
                        DynamicPartitionControlAndroidGroupTestP,
                        testing::Values(TestParam{0, 1}, TestParam{1, 0}));

const PartitionSuffixSizes update_sizes_0() {
  // Initial state is 0 for "other" slot.
  return {
      {"grown_a", 2_GiB},
      {"shrunk_a", 1_GiB},
      {"same_a", 100_MiB},
      {"deleted_a", 150_MiB},
      // no added_a
      {"grown_b", 200_MiB},
      // simulate system_other
      {"shrunk_b", 0},
      {"same_b", 0},
      {"deleted_b", 0},
      // no added_b
  };
}

const PartitionSuffixSizes update_sizes_1() {
  return {
      {"grown_a", 2_GiB},
      {"shrunk_a", 1_GiB},
      {"same_a", 100_MiB},
      {"deleted_a", 150_MiB},
      // no added_a
      {"grown_b", 3_GiB},
      {"shrunk_b", 150_MiB},
      {"same_b", 100_MiB},
      {"added_b", 150_MiB},
      // no deleted_b
  };
}

const PartitionSuffixSizes update_sizes_2() {
  return {
      {"grown_a", 4_GiB},
      {"shrunk_a", 100_MiB},
      {"same_a", 100_MiB},
      {"deleted_a", 64_MiB},
      // no added_a
      {"grown_b", 3_GiB},
      {"shrunk_b", 150_MiB},
      {"same_b", 100_MiB},
      {"added_b", 150_MiB},
      // no deleted_b
  };
}

// Test case for first update after the device is manufactured, in which
// case the "other" slot is likely of size "0" (except system, which is
// non-zero because of system_other partition)
TEST_F(DynamicPartitionControlAndroidTest, SimulatedFirstUpdate) {
  SetSlots({0, 1});

  SetMetadata(source(), update_sizes_0());
  SetMetadata(target(), update_sizes_0());
  ExpectStoreMetadata(update_sizes_1());
  ExpectUnmap({"grown_b", "shrunk_b", "same_b", "added_b"});

  EXPECT_TRUE(PreparePartitionsForUpdate({{"grown", 3_GiB},
                                          {"shrunk", 150_MiB},
                                          {"same", 100_MiB},
                                          {"added", 150_MiB}}));
}

// After first update, test for the second update. In the second update, the
// "added" partition is deleted and "deleted" partition is re-added.
TEST_F(DynamicPartitionControlAndroidTest, SimulatedSecondUpdate) {
  SetSlots({1, 0});

  SetMetadata(source(), update_sizes_1());
  SetMetadata(target(), update_sizes_0());

  ExpectStoreMetadata(update_sizes_2());
  ExpectUnmap({"grown_a", "shrunk_a", "same_a", "deleted_a"});

  EXPECT_TRUE(PreparePartitionsForUpdate({{"grown", 4_GiB},
                                          {"shrunk", 100_MiB},
                                          {"same", 100_MiB},
                                          {"deleted", 64_MiB}}));
}

TEST_F(DynamicPartitionControlAndroidTest, ApplyingToCurrentSlot) {
  SetSlots({1, 1});
  EXPECT_FALSE(PreparePartitionsForUpdate({}))
      << "Should not be able to apply to current slot.";
}

TEST_P(DynamicPartitionControlAndroidTestP, OptimizeOperationTest) {
  ASSERT_TRUE(dynamicControl().PreparePartitionsForUpdate(
      source(),
      target(),
      PartitionSizesToManifest({{"foo", 4_MiB}}),
      false,
      nullptr));
  dynamicControl().set_fake_mapped_devices({T("foo")});

  InstallOperation iop;
  InstallOperation optimized;
  Extent *se, *de;

  // Not a SOURCE_COPY operation, cannot skip.
  iop.set_type(InstallOperation::REPLACE);
  EXPECT_FALSE(dynamicControl().OptimizeOperation("foo", iop, &optimized));

  iop.set_type(InstallOperation::SOURCE_COPY);

  // By default GetVirtualAbFeatureFlag is disabled. Cannot skip operation.
  EXPECT_FALSE(dynamicControl().OptimizeOperation("foo", iop, &optimized));

  // Enable GetVirtualAbFeatureFlag in the mock interface.
  ON_CALL(dynamicControl(), GetVirtualAbFeatureFlag())
      .WillByDefault(Return(FeatureFlag(FeatureFlag::Value::LAUNCH)));

  // By default target_supports_snapshot_ is set to false. Cannot skip
  // operation.
  EXPECT_FALSE(dynamicControl().OptimizeOperation("foo", iop, &optimized));

  SetSnapshotEnabled(true);

  // Empty source and destination. Skip.
  EXPECT_TRUE(dynamicControl().OptimizeOperation("foo", iop, &optimized));
  EXPECT_TRUE(optimized.src_extents().empty());
  EXPECT_TRUE(optimized.dst_extents().empty());

  se = iop.add_src_extents();
  se->set_start_block(0);
  se->set_num_blocks(1);

  // There is something in sources, but destinations are empty. Cannot skip.
  EXPECT_FALSE(dynamicControl().OptimizeOperation("foo", iop, &optimized));

  InstallOperation iop2;

  de = iop2.add_dst_extents();
  de->set_start_block(0);
  de->set_num_blocks(1);

  // There is something in destinations, but sources are empty. Cannot skip.
  EXPECT_FALSE(dynamicControl().OptimizeOperation("foo", iop2, &optimized));

  de = iop.add_dst_extents();
  de->set_start_block(0);
  de->set_num_blocks(1);

  // Sources and destinations are identical. Skip.
  EXPECT_TRUE(dynamicControl().OptimizeOperation("foo", iop, &optimized));
  EXPECT_TRUE(optimized.src_extents().empty());
  EXPECT_TRUE(optimized.dst_extents().empty());

  se = iop.add_src_extents();
  se->set_start_block(1);
  se->set_num_blocks(5);

  // There is something in source, but not in destination. Cannot skip.
  EXPECT_FALSE(dynamicControl().OptimizeOperation("foo", iop, &optimized));

  de = iop.add_dst_extents();
  de->set_start_block(1);
  de->set_num_blocks(5);

  // There is source and destination are equal. Skip.
  EXPECT_TRUE(dynamicControl().OptimizeOperation("foo", iop, &optimized));
  EXPECT_TRUE(optimized.src_extents().empty());
  EXPECT_TRUE(optimized.dst_extents().empty());

  de = iop.add_dst_extents();
  de->set_start_block(6);
  de->set_num_blocks(5);

  // There is something extra in dest. Cannot skip.
  EXPECT_FALSE(dynamicControl().OptimizeOperation("foo", iop, &optimized));

  se = iop.add_src_extents();
  se->set_start_block(6);
  se->set_num_blocks(5);

  // Source and dest are identical again. Skip.
  EXPECT_TRUE(dynamicControl().OptimizeOperation("foo", iop, &optimized));
  EXPECT_TRUE(optimized.src_extents().empty());
  EXPECT_TRUE(optimized.dst_extents().empty());

  iop.Clear();
  iop.set_type(InstallOperation::SOURCE_COPY);
  se = iop.add_src_extents();
  se->set_start_block(1);
  se->set_num_blocks(1);
  se = iop.add_src_extents();
  se->set_start_block(3);
  se->set_num_blocks(2);
  se = iop.add_src_extents();
  se->set_start_block(7);
  se->set_num_blocks(2);
  de = iop.add_dst_extents();
  de->set_start_block(2);
  de->set_num_blocks(5);

  // [1, 3, 4, 7, 8] -> [2, 3, 4, 5, 6] should return [1, 7, 8] -> [2, 5, 6]
  EXPECT_TRUE(dynamicControl().OptimizeOperation("foo", iop, &optimized));
  ASSERT_EQ(2, optimized.src_extents_size());
  ASSERT_EQ(2, optimized.dst_extents_size());
  EXPECT_EQ(1u, optimized.src_extents(0).start_block());
  EXPECT_EQ(1u, optimized.src_extents(0).num_blocks());
  EXPECT_EQ(2u, optimized.dst_extents(0).start_block());
  EXPECT_EQ(1u, optimized.dst_extents(0).num_blocks());
  EXPECT_EQ(7u, optimized.src_extents(1).start_block());
  EXPECT_EQ(2u, optimized.src_extents(1).num_blocks());
  EXPECT_EQ(5u, optimized.dst_extents(1).start_block());
  EXPECT_EQ(2u, optimized.dst_extents(1).num_blocks());

  // Don't skip for static partitions.
  EXPECT_FALSE(dynamicControl().OptimizeOperation("bar", iop, &optimized));
}

TEST_F(DynamicPartitionControlAndroidTest, ResetUpdate) {
  MockPrefs prefs;
  ASSERT_TRUE(dynamicControl().ResetUpdate(&prefs));
}

TEST_F(DynamicPartitionControlAndroidTest, IsAvbNotEnabledInFstab) {
  // clang-format off
  std::string fstab_content =
      "system /postinstall ext4 ro,nosuid,nodev,noexec slotselect_other,logical\n"  // NOLINT(whitespace/line_length)
      "/dev/block/by-name/system /postinstall ext4 ro,nosuid,nodev,noexec slotselect_other\n";  // NOLINT(whitespace/line_length)
  // clang-format on
  ScopedTempFile fstab;
  ASSERT_TRUE(test_utils::WriteFileString(fstab.path(), fstab_content));
  ASSERT_THAT(dynamicControl().RealIsAvbEnabledInFstab(fstab.path()),
              Optional(false));
}

TEST_F(DynamicPartitionControlAndroidTest, IsAvbEnabledInFstab) {
  // clang-format off
  std::string fstab_content =
      "system /postinstall ext4 ro,nosuid,nodev,noexec slotselect_other,logical,avb_keys=/foo\n";  // NOLINT(whitespace/line_length)
  // clang-format on
  ScopedTempFile fstab;
  ASSERT_TRUE(test_utils::WriteFileString(fstab.path(), fstab_content));
  ASSERT_THAT(dynamicControl().RealIsAvbEnabledInFstab(fstab.path()),
              Optional(true));
}

TEST_P(DynamicPartitionControlAndroidTestP, AvbNotEnabledOnSystemOther) {
  ON_CALL(dynamicControl(), GetSystemOtherPath(_, _, _, _, _))
      .WillByDefault(Invoke([&](auto source_slot,
                                auto target_slot,
                                const auto& name,
                                auto path,
                                auto should_unmap) {
        return dynamicControl().RealGetSystemOtherPath(
            source_slot, target_slot, name, path, should_unmap);
      }));
  ON_CALL(dynamicControl(), IsAvbEnabledOnSystemOther())
      .WillByDefault(Return(false));
  EXPECT_TRUE(
      dynamicControl().RealEraseSystemOtherAvbFooter(source(), target()));
}

TEST_P(DynamicPartitionControlAndroidTestP, NoSystemOtherToErase) {
  SetMetadata(source(), {{S("system"), 100_MiB}});
  ON_CALL(dynamicControl(), IsAvbEnabledOnSystemOther())
      .WillByDefault(Return(true));
  std::string path;
  bool should_unmap;
  ASSERT_TRUE(dynamicControl().RealGetSystemOtherPath(
      source(), target(), T("system"), &path, &should_unmap));
  ASSERT_TRUE(path.empty()) << path;
  ASSERT_FALSE(should_unmap);
  ON_CALL(dynamicControl(), GetSystemOtherPath(_, _, _, _, _))
      .WillByDefault(Invoke([&](auto source_slot,
                                auto target_slot,
                                const auto& name,
                                auto path,
                                auto should_unmap) {
        return dynamicControl().RealGetSystemOtherPath(
            source_slot, target_slot, name, path, should_unmap);
      }));
  EXPECT_TRUE(
      dynamicControl().RealEraseSystemOtherAvbFooter(source(), target()));
}

TEST_P(DynamicPartitionControlAndroidTestP, SkipEraseUpdatedSystemOther) {
  PartitionSuffixSizes sizes{{S("system"), 100_MiB}, {T("system"), 100_MiB}};
  SetMetadata(source(), sizes, LP_PARTITION_ATTR_UPDATED);
  ON_CALL(dynamicControl(), IsAvbEnabledOnSystemOther())
      .WillByDefault(Return(true));
  std::string path;
  bool should_unmap;
  ASSERT_TRUE(dynamicControl().RealGetSystemOtherPath(
      source(), target(), T("system"), &path, &should_unmap));
  ASSERT_TRUE(path.empty()) << path;
  ASSERT_FALSE(should_unmap);
  ON_CALL(dynamicControl(), GetSystemOtherPath(_, _, _, _, _))
      .WillByDefault(Invoke([&](auto source_slot,
                                auto target_slot,
                                const auto& name,
                                auto path,
                                auto should_unmap) {
        return dynamicControl().RealGetSystemOtherPath(
            source_slot, target_slot, name, path, should_unmap);
      }));
  EXPECT_TRUE(
      dynamicControl().RealEraseSystemOtherAvbFooter(source(), target()));
}

TEST_P(DynamicPartitionControlAndroidTestP, EraseSystemOtherAvbFooter) {
  constexpr uint64_t file_size = 1_MiB;
  static_assert(file_size > AVB_FOOTER_SIZE);
  ScopedTempFile system_other;
  brillo::Blob original(file_size, 'X');
  ASSERT_TRUE(test_utils::WriteFileVector(system_other.path(), original));
  std::string mnt_path;
  ScopedLoopbackDeviceBinder dev(system_other.path(), true, &mnt_path);
  ASSERT_TRUE(dev.is_bound());

  brillo::Blob device_content;
  ASSERT_TRUE(utils::ReadFile(mnt_path, &device_content));
  ASSERT_EQ(original, device_content);

  PartitionSuffixSizes sizes{{S("system"), 100_MiB}, {T("system"), file_size}};
  SetMetadata(source(), sizes);
  ON_CALL(dynamicControl(), IsAvbEnabledOnSystemOther())
      .WillByDefault(Return(true));
  EXPECT_CALL(dynamicControl(),
              GetSystemOtherPath(source(), target(), T("system"), _, _))
      .WillRepeatedly(
          Invoke([&](auto, auto, const auto&, auto path, auto should_unmap) {
            *path = mnt_path;
            *should_unmap = false;
            return true;
          }));
  ASSERT_TRUE(
      dynamicControl().RealEraseSystemOtherAvbFooter(source(), target()));

  device_content.clear();
  ASSERT_TRUE(utils::ReadFile(mnt_path, &device_content));
  brillo::Blob new_expected(original);
  // Clear the last AVB_FOOTER_SIZE bytes.
  new_expected.resize(file_size - AVB_FOOTER_SIZE);
  new_expected.resize(file_size, '\0');
  ASSERT_EQ(new_expected, device_content);
}

}  // namespace chromeos_update_engine
