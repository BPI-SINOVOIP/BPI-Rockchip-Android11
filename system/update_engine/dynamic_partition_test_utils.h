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

#ifndef UPDATE_ENGINE_DYNAMIC_PARTITION_TEST_UTILS_H_
#define UPDATE_ENGINE_DYNAMIC_PARTITION_TEST_UTILS_H_

#include <stdint.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/strings/string_util.h>
#include <fs_mgr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <liblp/builder.h>
#include <storage_literals/storage_literals.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

using android::fs_mgr::MetadataBuilder;
using testing::_;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using namespace android::storage_literals;  // NOLINT(build/namespaces)

constexpr const uint32_t kMaxNumSlots = 2;
constexpr const char* kSlotSuffixes[kMaxNumSlots] = {"_a", "_b"};
constexpr const char* kFakeDevicePath = "/fake/dev/path/";
constexpr const char* kFakeDmDevicePath = "/fake/dm/dev/path/";
constexpr const uint32_t kFakeMetadataSize = 65536;
constexpr const char* kDefaultGroup = "foo";
constexpr const char* kFakeSuper = "fake_super";

// A map describing the size of each partition.
// "{name, size}"
using PartitionSizes = std::map<std::string, uint64_t>;

// "{name_a, size}"
using PartitionSuffixSizes = std::map<std::string, uint64_t>;

constexpr uint64_t kDefaultGroupSize = 5_GiB;
// Super device size. 1 MiB for metadata.
constexpr uint64_t kDefaultSuperSize = kDefaultGroupSize * 2 + 1_MiB;

template <typename U, typename V>
inline std::ostream& operator<<(std::ostream& os, const std::map<U, V>& param) {
  os << "{";
  bool first = true;
  for (const auto& pair : param) {
    if (!first)
      os << ", ";
    os << pair.first << ":" << pair.second;
    first = false;
  }
  return os << "}";
}

template <typename V>
inline void VectorToStream(std::ostream& os, const V& param) {
  os << "[";
  bool first = true;
  for (const auto& e : param) {
    if (!first)
      os << ", ";
    os << e;
    first = false;
  }
  os << "]";
}

inline std::ostream& operator<<(std::ostream& os, const PartitionUpdate& p) {
  return os << "{" << p.partition_name() << ", "
            << p.new_partition_info().size() << "}";
}

inline std::ostream& operator<<(std::ostream& os,
                                const DynamicPartitionGroup& g) {
  os << "{" << g.name() << ", " << g.size() << ", ";
  VectorToStream(os, g.partition_names());
  return os << "}";
}

inline std::ostream& operator<<(std::ostream& os,
                                const DeltaArchiveManifest& m) {
  os << "{.groups = ";
  VectorToStream(os, m.dynamic_partition_metadata().groups());
  os << ", .partitions = ";
  VectorToStream(os, m.partitions());
  return os;
}

inline std::string GetDevice(const std::string& name) {
  return kFakeDevicePath + name;
}

inline std::string GetDmDevice(const std::string& name) {
  return kFakeDmDevicePath + name;
}

inline DynamicPartitionGroup* AddGroup(DeltaArchiveManifest* manifest,
                                       const std::string& group,
                                       uint64_t group_size) {
  auto* g = manifest->mutable_dynamic_partition_metadata()->add_groups();
  g->set_name(group);
  g->set_size(group_size);
  return g;
}

inline void AddPartition(DeltaArchiveManifest* manifest,
                         DynamicPartitionGroup* group,
                         const std::string& partition,
                         uint64_t partition_size) {
  group->add_partition_names(partition);
  auto* p = manifest->add_partitions();
  p->set_partition_name(partition);
  p->mutable_new_partition_info()->set_size(partition_size);
}

// To support legacy tests, auto-convert {name_a: size} map to
// DeltaArchiveManifest.
inline DeltaArchiveManifest PartitionSuffixSizesToManifest(
    const PartitionSuffixSizes& partition_sizes) {
  DeltaArchiveManifest manifest;
  for (const char* suffix : kSlotSuffixes) {
    AddGroup(&manifest, std::string(kDefaultGroup) + suffix, kDefaultGroupSize);
  }
  for (const auto& pair : partition_sizes) {
    for (size_t suffix_idx = 0; suffix_idx < kMaxNumSlots; ++suffix_idx) {
      if (base::EndsWith(pair.first,
                         kSlotSuffixes[suffix_idx],
                         base::CompareCase::SENSITIVE)) {
        AddPartition(
            &manifest,
            manifest.mutable_dynamic_partition_metadata()->mutable_groups(
                suffix_idx),
            pair.first,
            pair.second);
      }
    }
  }
  return manifest;
}

// To support legacy tests, auto-convert {name: size} map to PartitionMetadata.
inline DeltaArchiveManifest PartitionSizesToManifest(
    const PartitionSizes& partition_sizes) {
  DeltaArchiveManifest manifest;
  auto* g = AddGroup(&manifest, std::string(kDefaultGroup), kDefaultGroupSize);
  for (const auto& pair : partition_sizes) {
    AddPartition(&manifest, g, pair.first, pair.second);
  }
  return manifest;
}

inline std::unique_ptr<MetadataBuilder> NewFakeMetadata(
    const DeltaArchiveManifest& manifest, uint32_t partition_attr = 0) {
  auto builder =
      MetadataBuilder::New(kDefaultSuperSize, kFakeMetadataSize, kMaxNumSlots);
  for (const auto& group : manifest.dynamic_partition_metadata().groups()) {
    EXPECT_TRUE(builder->AddGroup(group.name(), group.size()));
    for (const auto& partition_name : group.partition_names()) {
      EXPECT_NE(
          nullptr,
          builder->AddPartition(partition_name, group.name(), partition_attr));
    }
  }
  for (const auto& partition : manifest.partitions()) {
    auto p = builder->FindPartition(partition.partition_name());
    EXPECT_TRUE(p && builder->ResizePartition(
                         p, partition.new_partition_info().size()));
  }
  return builder;
}

class MetadataMatcher : public MatcherInterface<MetadataBuilder*> {
 public:
  explicit MetadataMatcher(const PartitionSuffixSizes& partition_sizes)
      : manifest_(PartitionSuffixSizesToManifest(partition_sizes)) {}
  explicit MetadataMatcher(const DeltaArchiveManifest& manifest)
      : manifest_(manifest) {}

  bool MatchAndExplain(MetadataBuilder* metadata,
                       MatchResultListener* listener) const override {
    bool success = true;
    for (const auto& group : manifest_.dynamic_partition_metadata().groups()) {
      for (const auto& partition_name : group.partition_names()) {
        auto p = metadata->FindPartition(partition_name);
        if (p == nullptr) {
          if (!success)
            *listener << "; ";
          *listener << "No partition " << partition_name;
          success = false;
          continue;
        }
        const auto& partition_updates = manifest_.partitions();
        auto it = std::find_if(partition_updates.begin(),
                               partition_updates.end(),
                               [&](const auto& p) {
                                 return p.partition_name() == partition_name;
                               });
        if (it == partition_updates.end()) {
          *listener << "Can't find partition update " << partition_name;
          success = false;
          continue;
        }
        auto partition_size = it->new_partition_info().size();
        if (p->size() != partition_size) {
          if (!success)
            *listener << "; ";
          *listener << "Partition " << partition_name << " has size "
                    << p->size() << ", expected " << partition_size;
          success = false;
        }
        if (p->group_name() != group.name()) {
          if (!success)
            *listener << "; ";
          *listener << "Partition " << partition_name << " has group "
                    << p->group_name() << ", expected " << group.name();
          success = false;
        }
      }
    }
    return success;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "expect: " << manifest_;
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "expect not: " << manifest_;
  }

 private:
  DeltaArchiveManifest manifest_;
};

inline Matcher<MetadataBuilder*> MetadataMatches(
    const PartitionSuffixSizes& partition_sizes) {
  return MakeMatcher(new MetadataMatcher(partition_sizes));
}

inline Matcher<MetadataBuilder*> MetadataMatches(
    const DeltaArchiveManifest& manifest) {
  return MakeMatcher(new MetadataMatcher(manifest));
}

MATCHER_P(HasGroup, group, " has group " + group) {
  auto groups = arg->ListGroups();
  return std::find(groups.begin(), groups.end(), group) != groups.end();
}

struct TestParam {
  uint32_t source;
  uint32_t target;
};
inline std::ostream& operator<<(std::ostream& os, const TestParam& param) {
  return os << "{source: " << param.source << ", target:" << param.target
            << "}";
}

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DYNAMIC_PARTITION_TEST_UTILS_H_
