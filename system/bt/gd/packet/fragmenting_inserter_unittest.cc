/*
 * Copyright 2019 The Android Open Source Project
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

#include "packet/fragmenting_inserter.h"

#include <gtest/gtest.h>
#include <memory>

#include "os/log.h"

using bluetooth::packet::FragmentingInserter;
using std::vector;

namespace bluetooth {
namespace packet {

TEST(FragmentingInserterTest, addMoreBits) {
  std::vector<uint8_t> result = {0b00011101 /* 3 2 1 */, 0b00010101 /* 5 4 */, 0b11100011 /* 7 6 */, 0b10000000 /* 8 */,
                                 0b10100000 /* filled with 1010 */};
  std::vector<std::unique_ptr<RawBuilder>> fragments;

  FragmentingInserter it(result.size(), std::back_insert_iterator(fragments));

  for (size_t i = 0; i < 9; i++) {
    it.insert_bits(static_cast<uint8_t>(i), i);
  }
  it.insert_bits(static_cast<uint8_t>(0b1010), 4);

  it.finalize();

  ASSERT_EQ(1, fragments.size());

  std::vector<uint8_t> bytes;
  BitInserter bit_inserter(bytes);
  fragments[0]->Serialize(bit_inserter);

  ASSERT_EQ(result.size(), bytes.size());
  for (size_t i = 0; i < bytes.size(); i++) {
    ASSERT_EQ(result[i], bytes[i]);
  }
}

TEST(FragmentingInserterTest, observerTest) {
  std::vector<uint8_t> result = {0b00011101 /* 3 2 1 */, 0b00010101 /* 5 4 */, 0b11100011 /* 7 6 */, 0b10000000 /* 8 */,
                                 0b10100000 /* filled with 1010 */};
  std::vector<std::unique_ptr<RawBuilder>> fragments;

  FragmentingInserter it(result.size() + 1, std::back_insert_iterator(fragments));

  std::vector<uint8_t> copy;

  uint64_t checksum = 0x0123456789abcdef;
  it.RegisterObserver(ByteObserver([&copy](uint8_t byte) { copy.push_back(byte); }, [checksum]() { return checksum; }));

  for (size_t i = 0; i < 9; i++) {
    it.insert_bits(static_cast<uint8_t>(i), i);
  }
  it.insert_bits(static_cast<uint8_t>(0b1010), 4);
  it.finalize();

  ASSERT_EQ(1, fragments.size());

  std::vector<uint8_t> bytes;
  BitInserter bit_inserter(bytes);
  fragments[0]->Serialize(bit_inserter);

  ASSERT_EQ(result.size(), bytes.size());
  for (size_t i = 0; i < bytes.size(); i++) {
    ASSERT_EQ(result[i], bytes[i]);
  }

  ASSERT_EQ(result.size(), copy.size());
  for (size_t i = 0; i < copy.size(); i++) {
    ASSERT_EQ(result[i], copy[i]);
  }

  ByteObserver observer = it.UnregisterObserver();
  ASSERT_EQ(checksum, observer.GetValue());
}

TEST(FragmentingInserterTest, testMtuBoundaries) {
  constexpr size_t kPacketSize = 1024;
  auto counts = RawBuilder();
  for (size_t i = 0; i < kPacketSize; i++) {
    counts.AddOctets1(static_cast<uint8_t>(i));
  }

  std::vector<std::unique_ptr<RawBuilder>> fragments_mtu_is_kPacketSize;
  FragmentingInserter it(kPacketSize, std::back_insert_iterator(fragments_mtu_is_kPacketSize));
  counts.Serialize(it);
  it.finalize();
  ASSERT_EQ(1, fragments_mtu_is_kPacketSize.size());
  ASSERT_EQ(kPacketSize, fragments_mtu_is_kPacketSize[0]->size());

  std::vector<std::unique_ptr<RawBuilder>> fragments_mtu_is_less;
  FragmentingInserter it_less(kPacketSize - 1, std::back_insert_iterator(fragments_mtu_is_less));
  counts.Serialize(it_less);
  it_less.finalize();
  ASSERT_EQ(2, fragments_mtu_is_less.size());
  ASSERT_EQ(kPacketSize - 1, fragments_mtu_is_less[0]->size());
  ASSERT_EQ(1, fragments_mtu_is_less[1]->size());

  std::vector<std::unique_ptr<RawBuilder>> fragments_mtu_is_more;
  FragmentingInserter it_more(kPacketSize + 1, std::back_insert_iterator(fragments_mtu_is_more));
  counts.Serialize(it_more);
  it_more.finalize();
  ASSERT_EQ(1, fragments_mtu_is_more.size());
  ASSERT_EQ(kPacketSize, fragments_mtu_is_more[0]->size());
}

constexpr size_t kPacketSize = 128;
class FragmentingTest : public ::testing::TestWithParam<size_t> {
 public:
  void StartUp() {
    counts_.reserve(kPacketSize);
    for (size_t i = 0; i < kPacketSize; i++) {
      counts_.push_back(static_cast<uint8_t>(i));
    }
  }
  void ShutDown() {
    counts_.clear();
  }
  std::vector<uint8_t> counts_;
};

TEST_P(FragmentingTest, mtuFragmentTest) {
  size_t mtu = GetParam();
  std::vector<std::unique_ptr<RawBuilder>> fragments;
  FragmentingInserter it(mtu, std::back_insert_iterator(fragments));

  RawBuilder original_packet(counts_);
  ASSERT_EQ(counts_.size(), original_packet.size());

  original_packet.Serialize(it);
  it.finalize();

  size_t expected_fragments = counts_.size() / mtu;
  if (counts_.size() % mtu != 0) {
    expected_fragments++;
  }
  ASSERT_EQ(expected_fragments, fragments.size());

  std::vector<std::vector<uint8_t>> serialized_fragments;
  for (size_t f = 0; f < fragments.size(); f++) {
    serialized_fragments.emplace_back(mtu);
    BitInserter bit_inserter(serialized_fragments[f]);
    fragments[f]->Serialize(bit_inserter);
    if (f + 1 == fragments.size() && (counts_.size() % mtu != 0)) {
      ASSERT_EQ(serialized_fragments[f].size(), counts_.size() % mtu);
    } else {
      ASSERT_EQ(serialized_fragments[f].size(), mtu);
    }
    for (size_t b = 0; b < serialized_fragments[f].size(); b++) {
      EXPECT_EQ(counts_[f * mtu + b], serialized_fragments[f][b]);
    }
  }
}

INSTANTIATE_TEST_CASE_P(chopomatic, FragmentingTest, ::testing::Range<size_t>(1, kPacketSize + 1));

}  // namespace packet
}  // namespace bluetooth
