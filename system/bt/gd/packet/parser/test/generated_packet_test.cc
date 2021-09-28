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

#define PACKET_TESTING
#include "packet/parser/test/big_endian_test_packets.h"
#include "packet/parser/test/test_packets.h"

#include <gtest/gtest.h>
#include <forward_list>
#include <memory>

#include "os/log.h"
#include "packet/bit_inserter.h"
#include "packet/parser/test/six_bytes.h"
#include "packet/raw_builder.h"

using ::bluetooth::packet::BitInserter;
using ::bluetooth::packet::kLittleEndian;
using ::bluetooth::packet::RawBuilder;
using ::bluetooth::packet::parser::test::SixBytes;
using std::vector;

namespace {
vector<uint8_t> child_two_two_three = {
    0x20 /* Reserved : 4, FourBits::TWO */,
    0x03 /* FourBits::THREE, Reserved : 4 */,
};
vector<uint8_t> child = {
    0x12 /* fixed */, 0x02 /* Size of the payload */, 0xa1 /* First byte of the payload */, 0xa2, 0xb1 /* footer */,
};
vector<uint8_t> child_with_six_bytes = {
    0x34 /* TwoBytes */,
    0x12,
    0xa1 /* First byte of the six_bytes */,
    0xa2,
    0xa3,
    0xa4,
    0xa5,
    0xa6,
    0xb1 /* Second six_bytes*/,
    0xb2,
    0xb3,
    0xb4,
    0xb5,
    0xb6,
};

}  // namespace

namespace bluetooth {
namespace packet {
namespace parser {
using namespace test;

TEST(GeneratedPacketTest, testChildTwoTwoThree) {
  auto packet = ChildTwoTwoThreeBuilder::Create();

  ASSERT_EQ(child_two_two_three.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), child_two_two_three.size());
  for (size_t i = 0; i < child_two_two_three.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), child_two_two_three[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentView wrong_view = ParentView::Create(packet_bytes_view);
  ASSERT_FALSE(wrong_view.IsValid());

  ParentTwoView parent_view = ParentTwoView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  ASSERT_EQ(FourBits::TWO, parent_view.GetFourBits());

  ChildTwoTwoView child_view = ChildTwoTwoView::Create(parent_view);
  ASSERT_TRUE(child_view.IsValid());
  ASSERT_EQ(FourBits::THREE, child_view.GetMoreBits());

  ChildTwoTwoThreeView grandchild_view = ChildTwoTwoThreeView::Create(child_view);
  ASSERT_TRUE(grandchild_view.IsValid());
}

TEST(GeneratedPacketTest, testChild) {
  uint16_t field_name = 0xa2a1;
  uint8_t footer = 0xb1;
  auto packet = ChildBuilder::Create(field_name, footer);

  ASSERT_EQ(child.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), child.size());
  for (size_t i = 0; i < child.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), child[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentView parent_view = ParentView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  auto payload = parent_view.GetPayload();

  ASSERT_EQ(child[1 /* skip fixed field */], payload.size());
  for (size_t i = 0; i < payload.size(); i++) {
    ASSERT_EQ(child[i + 2 /* fixed & size */], payload[i]);
  }

  ChildView child_view = ChildView::Create(parent_view);
  ASSERT_TRUE(child_view.IsValid());

  ASSERT_EQ(field_name, child_view.GetFieldName());
}

TEST(GeneratedPacketTest, testValidateWayTooSmall) {
  std::vector<uint8_t> too_small_bytes = {0x34};
  auto too_small = std::make_shared<std::vector<uint8_t>>(too_small_bytes.begin(), too_small_bytes.end());

  ParentWithSixBytesView invalid_parent = ParentWithSixBytesView::Create(too_small);
  ASSERT_FALSE(invalid_parent.IsValid());
  ChildWithSixBytesView invalid = ChildWithSixBytesView::Create(ParentWithSixBytesView::Create(too_small));
  ASSERT_FALSE(invalid.IsValid());
}

TEST(GeneratedPacketTest, testValidateTooSmall) {
  std::vector<uint8_t> too_small_bytes = {0x34, 0x12, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x11};
  auto too_small = std::make_shared<std::vector<uint8_t>>(too_small_bytes.begin(), too_small_bytes.end());

  ParentWithSixBytesView valid_parent = ParentWithSixBytesView::Create(too_small);
  ASSERT_TRUE(valid_parent.IsValid());
  ChildWithSixBytesView invalid = ChildWithSixBytesView::Create(ParentWithSixBytesView::Create(too_small));
  ASSERT_FALSE(invalid.IsValid());
}

TEST(GeneratedPacketTest, testValidateJustRight) {
  std::vector<uint8_t> just_right_bytes = {0x34, 0x12, 0x01, 0x02, 0x03, 0x04, 0x05,
                                           0x06, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16};
  auto just_right = std::make_shared<std::vector<uint8_t>>(just_right_bytes.begin(), just_right_bytes.end());

  ChildWithSixBytesView valid = ChildWithSixBytesView::Create(ParentWithSixBytesView::Create(just_right));
  ASSERT_TRUE(valid.IsValid());
}

TEST(GeneratedPacketTest, testValidateTooBig) {
  std::vector<uint8_t> too_big_bytes = {0x34, 0x12, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x20};
  auto too_big = std::make_shared<std::vector<uint8_t>>(too_big_bytes.begin(), too_big_bytes.end());

  ChildWithSixBytesView lenient = ChildWithSixBytesView::Create(ParentWithSixBytesView::Create(too_big));
  ASSERT_TRUE(lenient.IsValid());
}

TEST(GeneratedPacketTest, testValidateDeath) {
  auto packet = ChildTwoTwoThreeBuilder::Create();

  ASSERT_EQ(child_two_two_three.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), child_two_two_three.size());
  for (size_t i = 0; i < child_two_two_three.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), child_two_two_three[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentView wrong_view = ParentView::Create(packet_bytes_view);
  ASSERT_DEATH(wrong_view.GetPayload(), "validated");
}

TEST(GeneratedPacketTest, testValidatedParentDeath) {
  uint16_t field_name = 0xa2a1;
  uint8_t footer = 0xb1;
  auto packet = ChildBuilder::Create(field_name, footer);

  ASSERT_EQ(child.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), child.size());
  for (size_t i = 0; i < child.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), child[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentView parent_view = ParentView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  auto payload = parent_view.GetPayload();

  ASSERT_EQ(child[1 /* skip fixed field */], payload.size());
  for (size_t i = 0; i < payload.size(); i++) {
    ASSERT_EQ(child[i + 2 /* fixed & size */], payload[i]);
  }

  ChildView child_view = ChildView::Create(parent_view);
  ASSERT_DEATH(child_view.GetFieldName(), "validated");
}

vector<uint8_t> middle_four_bits = {
    0x95,  // low_two = ONE, next_four = FIVE, straddle = TEN
    0x8a,  // straddle = TEN, four_more = TWO, high_two = TWO
};

TEST(GeneratedPacketTest, testMiddleFourBitsPacket) {
  TwoBits low_two = TwoBits::ONE;
  FourBits next_four = FourBits::FIVE;
  FourBits straddle = FourBits::TEN;
  FourBits four_more = FourBits::TWO;
  TwoBits high_two = TwoBits::TWO;

  auto packet = MiddleFourBitsBuilder::Create(low_two, next_four, straddle, four_more, high_two);

  ASSERT_EQ(middle_four_bits.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), middle_four_bits.size());
  for (size_t i = 0; i < middle_four_bits.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), middle_four_bits[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  MiddleFourBitsView view = MiddleFourBitsView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(low_two, view.GetLowTwo());
  ASSERT_EQ(next_four, view.GetNextFour());
  ASSERT_EQ(straddle, view.GetStraddle());
  ASSERT_EQ(four_more, view.GetFourMore());
  ASSERT_EQ(high_two, view.GetHighTwo());
}

TEST(GeneratedPacketTest, testChildWithSixBytes) {
  SixBytes six_bytes_a{{0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6}};
  SixBytes six_bytes_b{{0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6}};
  auto packet = ChildWithSixBytesBuilder::Create(six_bytes_a, six_bytes_b);

  ASSERT_EQ(child_with_six_bytes.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), child_with_six_bytes.size());
  for (size_t i = 0; i < child_with_six_bytes.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), child_with_six_bytes[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentWithSixBytesView parent_view = ParentWithSixBytesView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  ASSERT_EQ(six_bytes_a, parent_view.GetSixBytes());

  ChildWithSixBytesView child_view = ChildWithSixBytesView::Create(parent_view);
  ASSERT_TRUE(child_view.IsValid());

  ASSERT_EQ(six_bytes_a, child_view.GetSixBytes());
  ASSERT_EQ(six_bytes_a, ((ParentWithSixBytesView)child_view).GetSixBytes());
  ASSERT_EQ(six_bytes_b, child_view.GetChildSixBytes());
}

namespace {
vector<uint8_t> parent_with_sum = {
    0x11 /* TwoBytes */, 0x12, 0x21 /* Sum Bytes */, 0x22, 0x43 /* Sum, excluding TwoBytes */, 0x00,
};

}  // namespace

TEST(GeneratedPacketTest, testParentWithSum) {
  uint16_t two_bytes = 0x1211;
  uint16_t sum_bytes = 0x2221;
  auto packet = ParentWithSumBuilder::Create(two_bytes, sum_bytes, std::make_unique<packet::RawBuilder>());

  ASSERT_EQ(parent_with_sum.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), parent_with_sum.size());
  for (size_t i = 0; i < parent_with_sum.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), parent_with_sum[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentWithSumView parent_view = ParentWithSumView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  ASSERT_EQ(two_bytes, parent_view.GetTwoBytes());

  // Corrupt checksum
  packet_bytes->back()++;
  PacketView<kLittleEndian> corrupted_bytes_view(packet_bytes);
  ParentWithSumView corrupted_view = ParentWithSumView::Create(corrupted_bytes_view);
  ASSERT_FALSE(corrupted_view.IsValid());
}

namespace {
vector<uint8_t> child_with_nested_sum = {
    0x11 /* TwoBytes */,
    0x12,
    0x21 /* Sum Bytes */,
    0x22,
    0x31 /* More Bytes */,
    0x32,
    0x33,
    0x34,
    0xca /* Nested Sum */,
    0x00,
    0xd7 /* Sum, excluding TwoBytes */,
    0x01,
};

}  // namespace

TEST(GeneratedPacketTest, testChildWithNestedSum) {
  uint16_t two_bytes = 0x1211;
  uint16_t sum_bytes = 0x2221;
  uint32_t more_bytes = 0x34333231;
  auto packet = ChildWithNestedSumBuilder::Create(two_bytes, sum_bytes, more_bytes);

  ASSERT_EQ(child_with_nested_sum.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(packet_bytes->size(), child_with_nested_sum.size());
  for (size_t i = 0; i < child_with_nested_sum.size(); i++) {
    ASSERT_EQ(packet_bytes->at(i), child_with_nested_sum[i]);
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentWithSumView parent_view = ParentWithSumView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  ASSERT_EQ(two_bytes, parent_view.GetTwoBytes());

  ChildWithNestedSumView child_view = ChildWithNestedSumView::Create(parent_view);
  ASSERT_TRUE(child_view.IsValid());

  ASSERT_EQ(more_bytes, child_view.GetMoreBytes());
}

namespace {
vector<uint8_t> parent_size_modifier = {
    0x02 /* Size */,
    0x11 /* TwoBytes */,
    0x12,
};

}  // namespace

TEST(GeneratedPacketTest, testParentSizeModifier) {
  uint16_t two_bytes = 0x1211;
  auto packet = ParentSizeModifierBuilder::Create(std::make_unique<RawBuilder>(), two_bytes);

  ASSERT_EQ(parent_size_modifier.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(parent_size_modifier.size(), packet_bytes->size());
  for (size_t i = 0; i < parent_size_modifier.size(); i++) {
    ASSERT_EQ(parent_size_modifier[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentSizeModifierView parent_view = ParentSizeModifierView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  ASSERT_EQ(two_bytes, parent_view.GetTwoBytes());
}

namespace {
vector<uint8_t> child_size_modifier = {
    0x06 /* PayloadSize (TwoBytes + MoreBytes)*/,
    0x31 /* MoreBytes */,
    0x32,
    0x33,
    0x34,
    0x11 /* TwoBytes = 0x1211 */,
    0x12,
};

}  // namespace

TEST(GeneratedPacketTest, testChildSizeModifier) {
  uint16_t two_bytes = 0x1211;
  uint32_t more_bytes = 0x34333231;
  auto packet = ChildSizeModifierBuilder::Create(more_bytes);

  ASSERT_EQ(child_size_modifier.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(child_size_modifier.size(), packet_bytes->size());
  for (size_t i = 0; i < child_size_modifier.size(); i++) {
    ASSERT_EQ(child_size_modifier[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  ParentSizeModifierView parent_view = ParentSizeModifierView::Create(packet_bytes_view);
  ASSERT_TRUE(parent_view.IsValid());
  ASSERT_EQ(two_bytes, parent_view.GetTwoBytes());

  ChildSizeModifierView child_view = ChildSizeModifierView::Create(parent_view);
  ASSERT_TRUE(child_view.IsValid());

  ASSERT_EQ(more_bytes, child_view.GetMoreBytes());
}

namespace {
vector<uint8_t> fixed_array_enum{
    0x01,  // ONE
    0x00,
    0x02,  // TWO
    0x00,
    0x01,  // ONE_TWO
    0x02,
    0x02,  // TWO_THREE
    0x03,
    0xff,  // FFFF
    0xff,
};
}

TEST(GeneratedPacketTest, testFixedArrayEnum) {
  std::array<ForArrays, 5> fixed_array{
      {ForArrays::ONE, ForArrays::TWO, ForArrays::ONE_TWO, ForArrays::TWO_THREE, ForArrays::FFFF}};
  auto packet = FixedArrayEnumBuilder::Create(fixed_array);
  ASSERT_EQ(fixed_array_enum.size(), packet->size());

  // Verify that the packet is independent from the array.
  std::array<ForArrays, 5> copy_array(fixed_array);
  fixed_array[1] = ForArrays::ONE;

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(fixed_array_enum.size(), packet_bytes->size());
  for (size_t i = 0; i < fixed_array_enum.size(); i++) {
    ASSERT_EQ(fixed_array_enum[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = FixedArrayEnumView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetEnumArray();
  ASSERT_EQ(copy_array.size(), array.size());
  for (size_t i = 0; i < copy_array.size(); i++) {
    ASSERT_EQ(array[i], copy_array[i]);
  }
}

namespace {
vector<uint8_t> sized_array_enum{
    0x0a,  // _size_
    0x00,
    0x01,  // ONE
    0x00,
    0x02,  // TWO
    0x00,
    0x01,  // ONE_TWO
    0x02,
    0x02,  // TWO_THREE
    0x03,
    0xff,  // FFFF
    0xff,
};
}

TEST(GeneratedPacketTest, testSizedArrayEnum) {
  std::vector<ForArrays> sized_array{
      {ForArrays::ONE, ForArrays::TWO, ForArrays::ONE_TWO, ForArrays::TWO_THREE, ForArrays::FFFF}};
  auto packet = SizedArrayEnumBuilder::Create(sized_array);
  ASSERT_EQ(sized_array_enum.size(), packet->size());

  // Copy the original vector and modify it to make sure the packet is independent.
  std::vector<ForArrays> copy_array(sized_array);
  sized_array[1] = ForArrays::ONE;

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(sized_array_enum.size(), packet_bytes->size());
  for (size_t i = 0; i < sized_array_enum.size(); i++) {
    ASSERT_EQ(sized_array_enum[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = SizedArrayEnumView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetEnumArray();
  ASSERT_EQ(copy_array.size(), array.size());
  for (size_t i = 0; i < copy_array.size(); i++) {
    ASSERT_EQ(array[i], copy_array[i]);
  }
}

namespace {
vector<uint8_t> count_array_enum{
    0x03,  // _count_
    0x01,  // ONE
    0x00,
    0x02,  // TWO_THREE
    0x03,
    0xff,  // FFFF
    0xff,
};
}

TEST(GeneratedPacketTest, testCountArrayEnum) {
  std::vector<ForArrays> count_array{{ForArrays::ONE, ForArrays::TWO_THREE, ForArrays::FFFF}};
  auto packet = CountArrayEnumBuilder::Create(count_array);
  ASSERT_EQ(count_array_enum.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(count_array_enum.size(), packet_bytes->size());
  for (size_t i = 0; i < count_array_enum.size(); i++) {
    ASSERT_EQ(count_array_enum[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = CountArrayEnumView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetEnumArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i], count_array[i]);
  }
}

TEST(GeneratedPacketTest, testFixedSizeByteArray) {
  constexpr std::size_t byte_array_size = 32;
  std::array<uint8_t, byte_array_size> byte_array;
  for (uint8_t i = 0; i < byte_array_size; i++) byte_array[i] = i;

  constexpr int word_array_size = 8;
  std::array<uint32_t, word_array_size> word_array;
  for (uint32_t i = 0; i < word_array_size; i++) word_array[i] = i;

  auto packet = PacketWithFixedArraysOfBytesBuilder::Create(byte_array, word_array);
  ASSERT_EQ(2 * (256 / 8), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(byte_array_size + word_array_size * sizeof(uint32_t), packet_bytes->size());

  for (size_t i = 0; i < byte_array_size; i++) {
    ASSERT_EQ(byte_array[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = PacketWithFixedArraysOfBytesView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetFixed256bitInBytes();
  ASSERT_EQ(byte_array.size(), array.size());
  for (size_t i = 0; i < array.size(); i++) {
    ASSERT_EQ(array[i], byte_array[i]);
  }

  auto decoded_word_array = view.GetFixed256bitInWords();
  ASSERT_EQ(word_array.size(), decoded_word_array.size());
  for (size_t i = 0; i < decoded_word_array.size(); i++) {
    ASSERT_EQ(word_array[i], decoded_word_array[i]);
  }
}

vector<uint8_t> one_variable{
    0x03, 'o', 'n', 'e',  // "one"
};

TEST(GeneratedPacketTest, testOneVariableField) {
  Variable variable_one{"one"};

  auto packet = OneVariableBuilder::Create(variable_one);
  ASSERT_EQ(one_variable.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_variable.size(), packet_bytes->size());
  for (size_t i = 0; i < one_variable.size(); i++) {
    ASSERT_EQ(one_variable[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one->data, variable_one.data);
}

vector<uint8_t> fou_variable{
    0x04, 'f', 'o', 'u',  // too short
};

TEST(GeneratedPacketTest, testOneVariableFieldTooShort) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>(fou_variable);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one, nullptr);
}

vector<uint8_t> sized_array_variable{
    0x0e,                           // _size_
    0x03, 'o', 'n', 'e',            // "one"
    0x03, 't', 'w', 'o',            // "two"
    0x05, 't', 'h', 'r', 'e', 'e',  // "three"
};

TEST(GeneratedPacketTest, testSizedArrayVariableLength) {
  std::vector<Variable> sized_array;
  sized_array.emplace_back("one");
  sized_array.emplace_back("two");
  sized_array.emplace_back("three");

  auto packet = SizedArrayVariableBuilder::Create(sized_array);
  ASSERT_EQ(sized_array_variable.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(sized_array_variable.size(), packet_bytes->size());
  for (size_t i = 0; i < sized_array_variable.size(); i++) {
    ASSERT_EQ(sized_array_variable[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = SizedArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(sized_array.size(), array.size());
  for (size_t i = 0; i < sized_array.size(); i++) {
    ASSERT_EQ(array[i].data, sized_array[i].data);
  }
}

vector<uint8_t> sized_array_variable_too_short{
    0x0e,                           // _size_
    0x03, 'o', 'n', 'e',            // "one"
    0x03, 't', 'w', 'o',            // "two"
    0x06, 't', 'h', 'r', 'e', 'e',  // "three" needs another letter to be length 6
};

TEST(GeneratedPacketTest, testSizedArrayVariableLengthLastBad) {
  std::vector<Variable> sized_array;
  sized_array.emplace_back("one");
  sized_array.emplace_back("two");

  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(sized_array_variable_too_short);

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = SizedArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(sized_array.size(), array.size());
  for (size_t i = 0; i < sized_array.size(); i++) {
    ASSERT_EQ(array[i].data, sized_array[i].data);
  }
}

vector<uint8_t> sized_array_variable_first_too_short{
    0x0e,                           // _size_
    0x02, 'o', 'n', 'e',            // "on"
    0x03, 't', 'w', 'o',            // "two"
    0x05, 't', 'h', 'r', 'e', 'e',  // "three"
};

TEST(GeneratedPacketTest, testSizedArrayVariableLengthFirstBad) {
  std::vector<Variable> sized_array;
  sized_array.emplace_back("on");

  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(sized_array_variable_first_too_short);

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = SizedArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(sized_array.size(), array.size());
  for (size_t i = 0; i < sized_array.size(); i++) {
    ASSERT_EQ(array[i].data, sized_array[i].data);
  }
}

vector<uint8_t> fixed_array_variable{
    0x03, 'o', 'n', 'e',            // "one"
    0x03, 't', 'w', 'o',            // "two"
    0x05, 't', 'h', 'r', 'e', 'e',  // "three"
    0x04, 'f', 'o', 'u', 'r',       // "four"
    0x04, 'f', 'i', 'v', 'e',       // "five"
};

TEST(GeneratedPacketTest, testFixedArrayVariableLength) {
  std::array<Variable, 5> fixed_array{std::string("one"), std::string("two"), std::string("three"), std::string("four"),
                                      std::string("five")};

  auto packet = FixedArrayVariableBuilder::Create(fixed_array);
  ASSERT_EQ(fixed_array_variable.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(fixed_array_variable.size(), packet_bytes->size());
  for (size_t i = 0; i < fixed_array_variable.size(); i++) {
    ASSERT_EQ(fixed_array_variable[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = FixedArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(fixed_array.size(), array.size());
  for (size_t i = 0; i < fixed_array.size(); i++) {
    ASSERT_EQ(array[i].data, fixed_array[i].data);
  }
}

vector<uint8_t> fixed_array_variable_too_short{
    0x03, 'o', 'n', 'e',            // "one"
    0x03, 't', 'w', 'o',            // "two"
    0x05, 't', 'h', 'r', 'e', 'e',  // "three"
    0x04, 'f', 'o', 'u', 'r',       // "four"
    0x05, 'f', 'i', 'v', 'e',       // "five"
};

TEST(GeneratedPacketTest, testFixedArrayVariableLengthTooShort) {
  std::array<Variable, 5> fixed_array{std::string("one"), std::string("two"), std::string("three"),
                                      std::string("four")};

  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(fixed_array_variable_too_short);

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = FixedArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(fixed_array.size(), array.size());
  for (size_t i = 0; i < fixed_array.size(); i++) {
    ASSERT_EQ(array[i].data, fixed_array[i].data);
  }
}

vector<uint8_t> count_array_variable{
    0x04,                           // _count_
    0x03, 'o', 'n', 'e',            // "one"
    0x03, 't', 'w', 'o',            // "two"
    0x05, 't', 'h', 'r', 'e', 'e',  // "three"
    0x04, 'f', 'o', 'u', 'r',       // "four"
};

TEST(GeneratedPacketTest, testCountArrayVariableLength) {
  std::vector<Variable> count_array;
  count_array.emplace_back("one");
  count_array.emplace_back("two");
  count_array.emplace_back("three");
  count_array.emplace_back("four");

  auto packet = CountArrayVariableBuilder::Create(count_array);
  ASSERT_EQ(count_array_variable.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(count_array_variable.size(), packet_bytes->size());
  for (size_t i = 0; i < count_array_variable.size(); i++) {
    ASSERT_EQ(count_array_variable[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = CountArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i].data, count_array[i].data);
  }
}

vector<uint8_t> count_array_variable_extra{
    0x04,                           // _count_
    0x03, 'o', 'n', 'e',            // "one"
    0x03, 't', 'w', 'o',            // "two"
    0x05, 't', 'h', 'r', 'e', 'e',  // "three"
    0x04, 'f', 'o', 'u', 'r',       // "four"
    0x04, 'x', 't', 'r', 'a',       // "xtra"
};

TEST(GeneratedPacketTest, testCountArrayVariableLengthExtraData) {
  std::vector<Variable> count_array;
  count_array.emplace_back("one");
  count_array.emplace_back("two");
  count_array.emplace_back("three");
  count_array.emplace_back("four");

  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(count_array_variable_extra);

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = CountArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i].data, count_array[i].data);
  }
}

vector<uint8_t> count_array_variable_too_few{
    0x04,                           // _count_
    0x03, 'o', 'n', 'e',            // "one"
    0x03, 't', 'w', 'o',            // "two"
    0x05, 't', 'h', 'r', 'e', 'e',  // "three"
};

TEST(GeneratedPacketTest, testCountArrayVariableLengthMissingData) {
  std::vector<Variable> count_array;
  count_array.emplace_back("one");
  count_array.emplace_back("two");
  count_array.emplace_back("three");

  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(count_array_variable_too_few);

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = CountArrayVariableView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetVariableArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i].data, count_array[i].data);
  }
}

vector<uint8_t> one_struct{
    0x01, 0x02, 0x03,  // id = 0x01, count = 0x0302
};

TEST(GeneratedPacketTest, testOneStruct) {
  TwoRelatedNumbers trn;
  trn.id_ = 1;
  trn.count_ = 0x0302;

  auto packet = OneStructBuilder::Create(trn);
  ASSERT_EQ(one_struct.size(), packet->size());

  // Copy the original struct, then modify it to verify independence from the packet.
  TwoRelatedNumbers copy_trn(trn);
  trn.id_ = 2;

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_struct.size(), packet_bytes->size());
  for (size_t i = 0; i < one_struct.size(); i++) {
    ASSERT_EQ(one_struct[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one.id_, copy_trn.id_);
  ASSERT_EQ(one.count_, copy_trn.count_);
}

vector<uint8_t> two_structs{
    0x01, 0x01, 0x02,  // id, id * 0x0201
    0x02, 0x02, 0x04,
};

TEST(GeneratedPacketTest, testTwoStructs) {
  std::vector<TwoRelatedNumbers> count_array;
  for (uint8_t i = 1; i < 3; i++) {
    TwoRelatedNumbers trn;
    trn.id_ = i;
    trn.count_ = 0x0201 * i;
    count_array.push_back(trn);
  }

  auto packet = TwoStructsBuilder::Create(count_array[0], count_array[1]);
  ASSERT_EQ(two_structs.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(two_structs.size(), packet_bytes->size());
  for (size_t i = 0; i < two_structs.size(); i++) {
    ASSERT_EQ(two_structs[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = TwoStructsView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one.id_, count_array[0].id_);
  ASSERT_EQ(one.count_, count_array[0].count_);
  auto two = view.GetTwo();
  ASSERT_EQ(two.id_, count_array[1].id_);
  ASSERT_EQ(two.count_, count_array[1].count_);
}

vector<uint8_t> array_or_vector_of_struct{
    0x04,              // _count_
    0x01, 0x01, 0x02,  // id, id * 0x0201
    0x02, 0x02, 0x04, 0x03, 0x03, 0x06, 0x04, 0x04, 0x08,
};

TEST(GeneratedPacketTest, testVectorOfStruct) {
  std::vector<TwoRelatedNumbers> count_array;
  for (uint8_t i = 1; i < 5; i++) {
    TwoRelatedNumbers trn;
    trn.id_ = i;
    trn.count_ = 0x0201 * i;
    count_array.push_back(trn);
  }

  // Make a copy
  std::vector<TwoRelatedNumbers> copy_array(count_array);

  auto packet = VectorOfStructBuilder::Create(count_array);

  // Change the original vector to make sure a copy was made.
  count_array[0].id_ = count_array[0].id_ + 1;

  ASSERT_EQ(array_or_vector_of_struct.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(array_or_vector_of_struct.size(), packet_bytes->size());
  for (size_t i = 0; i < array_or_vector_of_struct.size(); i++) {
    ASSERT_EQ(array_or_vector_of_struct[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = VectorOfStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetArray();
  ASSERT_EQ(copy_array.size(), array.size());
  for (size_t i = 0; i < copy_array.size(); i++) {
    ASSERT_EQ(array[i].id_, copy_array[i].id_);
    ASSERT_EQ(array[i].count_, copy_array[i].count_);
  }
}

TEST(GeneratedPacketTest, testArrayOfStruct) {
  std::array<TwoRelatedNumbers, 4> count_array;
  for (uint8_t i = 1; i < 5; i++) {
    TwoRelatedNumbers trn;
    trn.id_ = i;
    trn.count_ = 0x0201 * i;
    count_array[i - 1] = trn;
  }

  // Make a copy
  std::array<TwoRelatedNumbers, 4> copy_array(count_array);

  auto packet = ArrayOfStructBuilder::Create(4, count_array);

  // Change the original vector to make sure a copy was made.
  count_array[0].id_ = count_array[0].id_ + 1;

  ASSERT_EQ(array_or_vector_of_struct.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(array_or_vector_of_struct.size(), packet_bytes->size());
  for (size_t i = 0; i < array_or_vector_of_struct.size(); i++) {
    ASSERT_EQ(array_or_vector_of_struct[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = ArrayOfStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetArray();
  ASSERT_EQ(copy_array.size(), array.size());
  for (size_t i = 0; i < copy_array.size(); i++) {
    ASSERT_EQ(array[i].id_, copy_array[i].id_);
    ASSERT_EQ(array[i].count_, copy_array[i].count_);
  }
}

vector<uint8_t> one_fixed_types_struct{
    0x05,                                // four_bits = FIVE, reserved
    0xf3,                                // _fixed_
    0x0d,                                // id = 0x0d
    0x01, 0x02, 0x03,                    // array = { 1, 2, 3}
    0x06, 0x01,                          // example_checksum
    0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,  // six_bytes
};

TEST(GeneratedPacketTest, testOneFixedTypesStruct) {
  StructWithFixedTypes swf;
  swf.four_bits_ = FourBits::FIVE;
  swf.id_ = 0x0d;
  swf.array_ = {{0x01, 0x02, 0x03}};
  swf.six_bytes_ = SixBytes{{0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6}};

  auto packet = OneFixedTypesStructBuilder::Create(swf);
  ASSERT_EQ(one_fixed_types_struct.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_fixed_types_struct.size(), packet_bytes->size());
  for (size_t i = 0; i < one_fixed_types_struct.size(); i++) {
    ASSERT_EQ(one_fixed_types_struct[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneFixedTypesStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one.four_bits_, swf.four_bits_);
  ASSERT_EQ(one.id_, swf.id_);
  ASSERT_EQ(one.array_, swf.array_);
  ASSERT_EQ(one.six_bytes_, swf.six_bytes_);
}

vector<uint8_t> array_of_struct_and_another{
    0x03,              // _count_
    0x01, 0x01, 0x02,  // id, id * 0x0201
    0x02, 0x02, 0x04,  // 2
    0x03, 0x03, 0x06,  // 3
    0x04, 0x04, 0x08,  // Another
};

TEST(GeneratedPacketTest, testArrayOfStructAndAnother) {
  std::vector<TwoRelatedNumbers> count_array;
  for (uint8_t i = 1; i < 4; i++) {
    TwoRelatedNumbers trn;
    trn.id_ = i;
    trn.count_ = 0x0201 * i;
    count_array.push_back(trn);
  }
  TwoRelatedNumbers another;
  another.id_ = 4;
  another.count_ = 0x0201 * 4;

  auto packet = ArrayOfStructAndAnotherBuilder::Create(count_array, another);
  ASSERT_EQ(array_or_vector_of_struct.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(array_of_struct_and_another.size(), packet_bytes->size());
  for (size_t i = 0; i < array_of_struct_and_another.size(); i++) {
    ASSERT_EQ(array_of_struct_and_another[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = ArrayOfStructAndAnotherView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i].id_, count_array[i].id_);
    ASSERT_EQ(array[i].count_, count_array[i].count_);
  }
  auto nother = view.GetAnother();
  ASSERT_EQ(nother.id_, another.id_);
  ASSERT_EQ(nother.count_, another.count_);
}

DEFINE_AND_INSTANTIATE_OneArrayOfStructAndAnotherStructReflectionTest(array_of_struct_and_another);

TEST(GeneratedPacketTest, testOneArrayOfStructAndAnotherStruct) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(array_of_struct_and_another);

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneArrayOfStructAndAnotherStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one.array_.size(), 3);
  ASSERT_EQ(one.another_.id_, 4);
  ASSERT_EQ(one.another_.count_, 0x0804);
}

vector<uint8_t> sized_array_of_struct_and_another{
    0x09,              // _size_
    0x01, 0x01, 0x02,  // id, id * 0x0201
    0x02, 0x02, 0x04,  // 2
    0x03, 0x03, 0x06,  // 3
    0x04, 0x04, 0x08,  // Another
};

DEFINE_AND_INSTANTIATE_OneSizedArrayOfStructAndAnotherStructReflectionTest(sized_array_of_struct_and_another);

vector<uint8_t> bit_field_group_packet{
    // seven_bits_ = 0x77, straddle_ = 0x5, five_bits_ = 0x15
    0xf7,  // 0x77 | (0x5 & 0x1) << 7
    0xaa,  //  0x15 << 3 | (0x5 >> 1)
};

TEST(GeneratedPacketTest, testBitFieldGroupPacket) {
  uint8_t seven_bits = 0x77;
  uint8_t straddle = 0x5;
  uint8_t five_bits = 0x15;

  auto packet = BitFieldGroupPacketBuilder::Create(seven_bits, straddle, five_bits);
  ASSERT_EQ(bit_field_group_packet.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(bit_field_group_packet.size(), packet_bytes->size());
  for (size_t i = 0; i < bit_field_group_packet.size(); i++) {
    ASSERT_EQ(bit_field_group_packet[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = BitFieldGroupPacketView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(seven_bits, view.GetSevenBits());
  ASSERT_EQ(straddle, view.GetStraddle());
  ASSERT_EQ(five_bits, view.GetFiveBits());
}

vector<uint8_t> bit_field_packet{
    // seven_bits_ = 0x77, straddle_ = 0x5, five_bits_ = 0x15
    0xf7,  // 0x77 | (0x5 & 0x1) << 7
    0xaa,  //  0x15 << 3 | (0x5 >> 1)
};

TEST(GeneratedPacketTest, testBitFieldPacket) {
  BitField bit_field;
  bit_field.seven_bits_ = 0x77;
  bit_field.straddle_ = 0x5;
  bit_field.five_bits_ = 0x15;

  auto packet = BitFieldPacketBuilder::Create(bit_field);
  ASSERT_EQ(bit_field_packet.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(bit_field_packet.size(), packet_bytes->size());
  for (size_t i = 0; i < bit_field_packet.size(); i++) {
    ASSERT_EQ(bit_field_packet[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = BitFieldPacketView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  BitField bf = view.GetBitField();
  ASSERT_EQ(bf.seven_bits_, bit_field.seven_bits_);
  ASSERT_EQ(bf.straddle_, bit_field.straddle_);
  ASSERT_EQ(bf.five_bits_, bit_field.five_bits_);
}

vector<uint8_t> bit_field_group_after_unsized_array_packet{
    0x01, 0x02, 0x03, 0x04,  // byte array
    // seven_bits_ = 0x77, straddle_ = 0x5, five_bits_ = 0x15
    0xf7,  // 0x77 | (0x5 & 0x1) << 7
    0xaa,  //  0x15 << 3 | (0x5 >> 1)
};

TEST(GeneratedPacketTest, testBitFieldGroupAfterUnsizedArrayPacket) {
  std::vector<uint8_t> count_array;
  for (uint8_t i = 1; i < 5; i++) {
    count_array.push_back(i);
  }
  uint8_t seven_bits = 0x77;
  uint8_t straddle = 0x5;
  uint8_t five_bits = 0x15;

  auto packet = BitFieldGroupAfterUnsizedArrayPacketBuilder::Create(count_array, seven_bits, straddle, five_bits);
  ASSERT_EQ(bit_field_group_after_unsized_array_packet.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(bit_field_group_after_unsized_array_packet.size(), packet_bytes->size());
  for (size_t i = 0; i < bit_field_group_after_unsized_array_packet.size(); i++) {
    ASSERT_EQ(bit_field_group_after_unsized_array_packet[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto payload_view = BitFieldGroupAfterPayloadPacketView::Create(packet_bytes_view);
  ASSERT_TRUE(payload_view.IsValid());
  EXPECT_EQ(seven_bits, payload_view.GetSevenBits());
  EXPECT_EQ(straddle, payload_view.GetStraddle());
  EXPECT_EQ(five_bits, payload_view.GetFiveBits());

  auto view = BitFieldGroupAfterUnsizedArrayPacketView::Create(payload_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i], count_array[i]);
  }
  ASSERT_EQ(seven_bits, view.GetSevenBits());
  ASSERT_EQ(straddle, view.GetStraddle());
  ASSERT_EQ(five_bits, view.GetFiveBits());
}

vector<uint8_t> bit_field_after_unsized_array_packet{
    0x01, 0x02, 0x03, 0x04,  // byte array
    // seven_bits_ = 0x77, straddle_ = 0x5, five_bits_ = 0x15
    0xf7,  // 0x77 | (0x5 & 0x1) << 7
    0xaa,  //  0x15 << 3 | (0x5 >> 1)
};

TEST(GeneratedPacketTest, testBitFieldAfterUnsizedArrayPacket) {
  std::vector<uint8_t> count_array;
  for (uint8_t i = 1; i < 5; i++) {
    count_array.push_back(i);
  }
  BitField bit_field;
  bit_field.seven_bits_ = 0x77;
  bit_field.straddle_ = 0x5;
  bit_field.five_bits_ = 0x15;

  auto packet = BitFieldAfterUnsizedArrayPacketBuilder::Create(count_array, bit_field);
  ASSERT_EQ(bit_field_after_unsized_array_packet.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(bit_field_after_unsized_array_packet.size(), packet_bytes->size());
  for (size_t i = 0; i < bit_field_after_unsized_array_packet.size(); i++) {
    ASSERT_EQ(bit_field_after_unsized_array_packet[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto payload_view = BitFieldAfterPayloadPacketView::Create(packet_bytes_view);
  ASSERT_TRUE(payload_view.IsValid());
  BitField parent_bf = payload_view.GetBitField();
  ASSERT_EQ(parent_bf.seven_bits_, bit_field.seven_bits_);
  ASSERT_EQ(parent_bf.straddle_, bit_field.straddle_);
  ASSERT_EQ(parent_bf.five_bits_, bit_field.five_bits_);

  auto view = BitFieldAfterUnsizedArrayPacketView::Create(payload_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i], count_array[i]);
  }
  BitField bf = view.GetBitField();
  ASSERT_EQ(bf.seven_bits_, bit_field.seven_bits_);
  ASSERT_EQ(bf.straddle_, bit_field.straddle_);
  ASSERT_EQ(bf.five_bits_, bit_field.five_bits_);
}

vector<uint8_t> bit_field_array_packet{
    0x06,  // _size_(array)
    // seven_bits_ = 0x77, straddle_ = 0x5, five_bits_ = 0x15
    0xf7,  // 0x77 | (0x5 & 0x1) << 7
    0xaa,  //  0x15 << 3 | (0x5 >> 1)

    // seven_bits_ = 0x78, straddle_ = 0x6, five_bits_ = 0x16
    0x78,  // 0x78 | (0x6 & 0x1) << 7
    0xb3,  //  0x16 << 3 | (0x6 >> 1)

    // seven_bits_ = 0x79, straddle_ = 0x7, five_bits_ = 0x17
    0xf9,  // 0x79 | (0x7 & 0x1) << 7
    0xbb,  //  0x17 << 3 | (0x7 >> 1)
};

TEST(GeneratedPacketTest, testBitFieldArrayPacket) {
  std::vector<BitField> count_array;
  for (size_t i = 0; i < 3; i++) {
    BitField bf;
    bf.seven_bits_ = 0x77 + i;
    bf.straddle_ = 0x5 + i;
    bf.five_bits_ = 0x15 + i;
    count_array.push_back(bf);
  }

  auto packet = BitFieldArrayPacketBuilder::Create(count_array);
  ASSERT_EQ(bit_field_array_packet.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(bit_field_array_packet.size(), packet_bytes->size());
  for (size_t i = 0; i < bit_field_array_packet.size(); i++) {
    ASSERT_EQ(bit_field_array_packet[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = BitFieldArrayPacketView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i].seven_bits_, count_array[i].seven_bits_);
    ASSERT_EQ(array[i].straddle_, count_array[i].straddle_);
    ASSERT_EQ(array[i].five_bits_, count_array[i].five_bits_);
  }
}

TEST(GeneratedPacketTest, testNewBitFieldArrayPacket) {
  PacketView<kLittleEndian> packet_bytes_view(std::make_shared<std::vector<uint8_t>>(bit_field_array_packet));
  auto view = BitFieldArrayPacketView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());

  auto packet = BitFieldArrayPacketBuilder::Create(view.GetArray());
  ASSERT_EQ(bit_field_array_packet.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(*packet_bytes, bit_field_array_packet);
}

std::vector<uint8_t> child_two_two_two_ = {0x20, 0x02};
std::vector<uint8_t> child_two_two_three_ = {0x20, 0x03};
std::vector<uint8_t> child_two_two_four_ = {0x20, 0x04};

DEFINE_AND_INSTANTIATE_ParentTwoReflectionTest(child_two_two_two_, child_two_two_three_, child_two_two_four_);

DEFINE_AND_INSTANTIATE_ChildTwoTwoReflectionTest(child_two_two_two_, child_two_two_three_, child_two_two_four_);

DEFINE_AND_INSTANTIATE_ChildTwoTwoThreeReflectionTest(child_two_two_three_);

std::vector<uint8_t> one_versionless_struct_packet = {0x01};
std::vector<uint8_t> one_versioned_struct_packet = {0x02, 0x03 /* version */, 0x04, 0x05, 0x06};
std::vector<uint8_t> one_version_one_struct_packet = {0x03, 0x01 /* version */, 0x02};
std::vector<uint8_t> one_version_two_struct_packet = {0x03, 0x02 /* version */, 0x03, 0x04};
DEFINE_AND_INSTANTIATE_OneVersionlessStructPacketReflectionTest(one_versionless_struct_packet,
                                                                one_versioned_struct_packet,
                                                                one_version_one_struct_packet,
                                                                one_version_two_struct_packet);
DEFINE_AND_INSTANTIATE_OneVersionedStructPacketReflectionTest(one_versioned_struct_packet,
                                                              one_version_one_struct_packet,
                                                              one_version_two_struct_packet);
DEFINE_AND_INSTANTIATE_OneVersionOneStructPacketReflectionTest(one_version_one_struct_packet);
DEFINE_AND_INSTANTIATE_OneVersionTwoStructPacketReflectionTest(one_version_two_struct_packet);

vector<uint8_t> one_struct_be{
    0x01, 0x02, 0x03,  // id = 0x01, count = 0x0203
};

TEST(GeneratedPacketTest, testOneStructBe) {
  TwoRelatedNumbersBe trn;
  trn.id_ = 1;
  trn.count_ = 0x0203;

  auto packet = OneStructBeBuilder::Create(trn);
  ASSERT_EQ(one_struct_be.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_struct_be.size(), packet_bytes->size());
  for (size_t i = 0; i < one_struct_be.size(); i++) {
    ASSERT_EQ(one_struct_be[i], packet_bytes->at(i));
  }

  PacketView<!kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneStructBeView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one.id_, trn.id_);
  ASSERT_EQ(one.count_, trn.count_);
}

vector<uint8_t> two_structs_be{
    0x01, 0x01, 0x02,  // id, id * 0x0102
    0x02, 0x02, 0x04,
};

TEST(GeneratedPacketTest, testTwoStructsBe) {
  std::vector<TwoRelatedNumbersBe> count_array;
  for (uint8_t i = 1; i < 3; i++) {
    TwoRelatedNumbersBe trn;
    trn.id_ = i;
    trn.count_ = 0x0102 * i;
    count_array.push_back(trn);
  }

  auto packet = TwoStructsBeBuilder::Create(count_array[0], count_array[1]);
  ASSERT_EQ(two_structs_be.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(two_structs_be.size(), packet_bytes->size());
  for (size_t i = 0; i < two_structs_be.size(); i++) {
    ASSERT_EQ(two_structs_be[i], packet_bytes->at(i));
  }

  PacketView<!kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = TwoStructsBeView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOne();
  ASSERT_EQ(one.id_, count_array[0].id_);
  ASSERT_EQ(one.count_, count_array[0].count_);
  auto two = view.GetTwo();
  ASSERT_EQ(two.id_, count_array[1].id_);
  ASSERT_EQ(two.count_, count_array[1].count_);
}

vector<uint8_t> array_of_struct_be{
    0x04,              // _count_
    0x01, 0x01, 0x02,  // id, id * 0x0102
    0x02, 0x02, 0x04, 0x03, 0x03, 0x06, 0x04, 0x04, 0x08,
};

TEST(GeneratedPacketTest, testArrayOfStructBe) {
  std::vector<TwoRelatedNumbersBe> count_array;
  for (uint8_t i = 1; i < 5; i++) {
    TwoRelatedNumbersBe trn;
    trn.id_ = i;
    trn.count_ = 0x0102 * i;
    count_array.push_back(trn);
  }

  auto packet = ArrayOfStructBeBuilder::Create(count_array);

  ASSERT_EQ(array_of_struct_be.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(array_of_struct_be.size(), packet_bytes->size());
  for (size_t i = 0; i < array_of_struct_be.size(); i++) {
    ASSERT_EQ(array_of_struct_be[i], packet_bytes->at(i));
  }

  PacketView<!kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = ArrayOfStructBeView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto array = view.GetArray();
  ASSERT_EQ(count_array.size(), array.size());
  for (size_t i = 0; i < count_array.size(); i++) {
    ASSERT_EQ(array[i].id_, count_array[i].id_);
    ASSERT_EQ(array[i].count_, count_array[i].count_);
  }
}

vector<uint8_t> one_four_byte_struct{
    0x04,                    // struct_type_ = FourByte
    0xd1, 0xd2, 0xd3, 0xd4,  // four_bytes_
};

TEST(GeneratedPacketTest, testOneFourByteStruct) {
  FourByteStruct four_byte_struct;
  four_byte_struct.four_bytes_ = 0xd4d3d2d1;

  auto packet = OneFourByteStructBuilder::Create(four_byte_struct);
  ASSERT_EQ(one_four_byte_struct.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_four_byte_struct.size(), packet_bytes->size());
  for (size_t i = 0; i < one_four_byte_struct.size(); i++) {
    ASSERT_EQ(one_four_byte_struct[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneFourByteStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(StructType::FOUR_BYTE, view.GetOneStruct().struct_type_);
  ASSERT_EQ(four_byte_struct.four_bytes_, view.GetOneStruct().four_bytes_);
}

vector<uint8_t> generic_struct_two{
    0x02,        // struct_type_ = TwoByte
    0x01, 0x02,  // two_bytes_
};

TEST(GeneratedPacketTest, testOneGenericStructTwo) {
  TwoByteStruct two_byte_struct;
  two_byte_struct.two_bytes_ = 0x0201;
  std::unique_ptr<TwoByteStruct> two_byte_struct_ptr = std::make_unique<TwoByteStruct>(two_byte_struct);

  auto packet = OneGenericStructBuilder::Create(std::move(two_byte_struct_ptr));
  ASSERT_EQ(generic_struct_two.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(generic_struct_two.size(), packet_bytes->size());
  for (size_t i = 0; i < generic_struct_two.size(); i++) {
    ASSERT_EQ(generic_struct_two[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneGenericStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto base_struct = view.GetBaseStruct();
  ASSERT_NE(nullptr, base_struct);
  ASSERT_TRUE(TwoByteStruct::IsInstance(*base_struct));
  TwoByteStruct* two_byte = static_cast<TwoByteStruct*>(base_struct.get());
  ASSERT_NE(nullptr, two_byte);
  ASSERT_TRUE(TwoByteStruct::IsInstance(*two_byte));
  ASSERT_EQ(two_byte_struct.two_bytes_, 0x0201);
  uint16_t val = two_byte->two_bytes_;
  ASSERT_EQ(val, 0x0201);
  ASSERT_EQ(two_byte_struct.two_bytes_, ((TwoByteStruct*)base_struct.get())->two_bytes_);
}

vector<uint8_t> generic_struct_four{
    0x04,                    // struct_type_ = FourByte
    0x01, 0x02, 0x03, 0x04,  // four_bytes_
};

TEST(GeneratedPacketTest, testOneGenericStructFour) {
  FourByteStruct four_byte_struct;
  four_byte_struct.four_bytes_ = 0x04030201;
  std::unique_ptr<FourByteStruct> four_byte_struct_p = std::make_unique<FourByteStruct>(four_byte_struct);
  ASSERT_EQ(four_byte_struct.four_bytes_, four_byte_struct_p->four_bytes_);

  auto packet = OneGenericStructBuilder::Create(std::move(four_byte_struct_p));
  ASSERT_EQ(generic_struct_four.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(generic_struct_four.size(), packet_bytes->size());
  for (size_t i = 0; i < generic_struct_four.size(); i++) {
    ASSERT_EQ(generic_struct_four[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneGenericStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto base_struct = view.GetBaseStruct();
  ASSERT_NE(nullptr, base_struct);
  ASSERT_EQ(StructType::FOUR_BYTE, base_struct->struct_type_);
  ASSERT_EQ(four_byte_struct.four_bytes_, ((FourByteStruct*)base_struct.get())->four_bytes_);
}

vector<uint8_t> one_struct_array{
    0x04,                    // struct_type_ = FourByte
    0xa1, 0xa2, 0xa3, 0xa4,  // four_bytes_
    0x04,                    // struct_type_ = FourByte
    0xb2, 0xb2, 0xb3, 0xb4,  // four_bytes_
    0x02,                    // struct_type_ = TwoByte
    0xc3, 0xc2,              // two_bytes_
    0x04,                    // struct_type_ = TwoByte
    0xd4, 0xd2, 0xd3, 0xd4,  // four_bytes_
};

TEST(GeneratedPacketTest, testOneGenericStructArray) {
  std::vector<std::unique_ptr<UnusedParentStruct>> parent_vector;
  std::unique_ptr<FourByteStruct> fbs;
  std::unique_ptr<TwoByteStruct> tbs;
  fbs = std::make_unique<FourByteStruct>();
  fbs->four_bytes_ = 0xa4a3a2a1;
  parent_vector.push_back(std::move(fbs));
  fbs = std::make_unique<FourByteStruct>();
  fbs->four_bytes_ = 0xb4b3b2b2;
  parent_vector.push_back(std::move(fbs));
  tbs = std::make_unique<TwoByteStruct>();
  tbs->two_bytes_ = 0xc2c3;
  parent_vector.push_back(std::move(tbs));
  fbs = std::make_unique<FourByteStruct>();
  fbs->four_bytes_ = 0xd4d3d2d4;
  parent_vector.push_back(std::move(fbs));

  std::vector<std::unique_ptr<UnusedParentStruct>> vector_copy;
  for (auto& s : parent_vector) {
    if (s->struct_type_ == StructType::TWO_BYTE) {
      vector_copy.push_back(std::make_unique<TwoByteStruct>(*(TwoByteStruct*)s.get()));
    }
    if (s->struct_type_ == StructType::FOUR_BYTE) {
      vector_copy.push_back(std::make_unique<FourByteStruct>(*(FourByteStruct*)s.get()));
    }
  }

  auto packet = OneGenericStructArrayBuilder::Create(std::move(parent_vector));
  ASSERT_EQ(one_struct_array.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_struct_array.size(), packet_bytes->size());
  for (size_t i = 0; i < one_struct_array.size(); i++) {
    ASSERT_EQ(one_struct_array[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneGenericStructArrayView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto an_array = view.GetAnArray();
  ASSERT_EQ(vector_copy.size(), an_array.size());
  for (size_t i = 0; i < vector_copy.size(); i++) {
    ASSERT_NE(nullptr, an_array[i]);
    ASSERT_EQ(vector_copy[i]->struct_type_, an_array[i]->struct_type_);
    if (vector_copy[i]->struct_type_ == StructType::FOUR_BYTE) {
      ASSERT_EQ(FourByteStruct::Specialize(vector_copy[i].get())->four_bytes_,
                FourByteStruct::Specialize(an_array[i].get())->four_bytes_);
    } else {
      ASSERT_EQ(TwoByteStruct::Specialize(vector_copy[i].get())->two_bytes_,
                TwoByteStruct::Specialize(an_array[i].get())->two_bytes_);
    }
  }
}

TEST(GeneratedPacketTest, testOneGenericStructFourArray) {
  std::array<std::unique_ptr<UnusedParentStruct>, 4> parent_vector;
  std::unique_ptr<FourByteStruct> fbs;
  std::unique_ptr<TwoByteStruct> tbs;
  fbs = std::make_unique<FourByteStruct>();
  fbs->four_bytes_ = 0xa4a3a2a1;
  parent_vector[0] = std::move(fbs);
  fbs = std::make_unique<FourByteStruct>();
  fbs->four_bytes_ = 0xb4b3b2b2;
  parent_vector[1] = std::move(fbs);
  tbs = std::make_unique<TwoByteStruct>();
  tbs->two_bytes_ = 0xc2c3;
  parent_vector[2] = std::move(tbs);
  fbs = std::make_unique<FourByteStruct>();
  fbs->four_bytes_ = 0xd4d3d2d4;
  parent_vector[3] = std::move(fbs);

  std::array<std::unique_ptr<UnusedParentStruct>, 4> vector_copy;
  size_t index = 0;
  for (auto& s : parent_vector) {
    if (s->struct_type_ == StructType::TWO_BYTE) {
      vector_copy[index] = std::make_unique<TwoByteStruct>(*(TwoByteStruct*)s.get());
    }
    if (s->struct_type_ == StructType::FOUR_BYTE) {
      vector_copy[index] = std::make_unique<FourByteStruct>(*(FourByteStruct*)s.get());
    }
    index++;
  }

  auto packet = OneGenericStructFourArrayBuilder::Create(std::move(parent_vector));
  ASSERT_EQ(one_struct_array.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_struct_array.size(), packet_bytes->size());
  for (size_t i = 0; i < one_struct_array.size(); i++) {
    ASSERT_EQ(one_struct_array[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneGenericStructFourArrayView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto an_array = view.GetAnArray();
  ASSERT_EQ(vector_copy.size(), an_array.size());
  for (size_t i = 0; i < vector_copy.size(); i++) {
    ASSERT_NE(nullptr, an_array[i]);
    ASSERT_EQ(vector_copy[i]->struct_type_, an_array[i]->struct_type_);
    if (vector_copy[i]->struct_type_ == StructType::FOUR_BYTE) {
      ASSERT_EQ(FourByteStruct::Specialize(vector_copy[i].get())->four_bytes_,
                FourByteStruct::Specialize(an_array[i].get())->four_bytes_);
    } else {
      ASSERT_EQ(TwoByteStruct::Specialize(vector_copy[i].get())->two_bytes_,
                TwoByteStruct::Specialize(an_array[i].get())->two_bytes_);
    }
  }
}

vector<uint8_t> one_struct_array_after_fixed{
    0x01, 0x02,              // two_bytes = 0x0201
    0x04,                    // struct_type_ = FourByte
    0xa1, 0xa2, 0xa3, 0xa4,  // four_bytes_
    0x04,                    // struct_type_ = FourByte
    0xb2, 0xb2, 0xb3, 0xb4,  // four_bytes_
    0x02,                    // struct_type_ = TwoByte
    0xc3, 0xc2,              // two_bytes_
    0x04,                    // struct_type_ = TwoByte
    0xd4, 0xd2, 0xd3, 0xd4,  // four_bytes_
};

DEFINE_AND_INSTANTIATE_OneGenericStructArrayAfterFixedReflectionTest(one_struct_array_after_fixed);

vector<uint8_t> one_length_type_value_struct{
    // _size_(value):16 type value
    0x04, 0x00, 0x01, 'o', 'n', 'e',            // ONE
    0x04, 0x00, 0x02, 't', 'w', 'o',            // TWO
    0x06, 0x00, 0x03, 't', 'h', 'r', 'e', 'e',  // THREE
};

DEFINE_AND_INSTANTIATE_OneLengthTypeValueStructReflectionTest(one_length_type_value_struct);

TEST(GeneratedPacketTest, testOneLengthTypeValueStruct) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes =
      std::make_shared<std::vector<uint8_t>>(one_length_type_value_struct);

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneLengthTypeValueStructView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  auto one = view.GetOneArray();
  size_t entry_id = 0;
  for (const auto& entry : one) {
    switch (entry_id++) {
      case 0:
        ASSERT_EQ(entry.type_, DataType::ONE);
        ASSERT_EQ(entry.value_, std::vector<uint8_t>({'o', 'n', 'e'}));
        break;
      case 1:
        ASSERT_EQ(entry.type_, DataType::TWO);
        ASSERT_EQ(entry.value_, std::vector<uint8_t>({'t', 'w', 'o'}));
        break;
      case 2:
        ASSERT_EQ(entry.type_, DataType::THREE);
        ASSERT_EQ(entry.value_, std::vector<uint8_t>({'t', 'h', 'r', 'e', 'e'}));
        break;
      default:
        ASSERT_EQ(entry.type_, DataType::UNUSED);
    }
  }
}

vector<uint8_t> one_length_type_value_struct_padded_20{
    0x27,  // _size_(payload),
    // _size_(value):16 type value
    0x04, 0x00, 0x01, 'o', 'n', 'e',                             // ONE
    0x04, 0x00, 0x02, 't', 'w', 'o',                             // TWO
    0x06, 0x00, 0x03, 't', 'h', 'r', 'e', 'e',                   // THREE
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,        // padding to 30
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // padding to 40
};

vector<uint8_t> one_length_type_value_struct_padded_28{
    0x27,  // _size_(payload),
    // _size_(value):16 type value
    0x04, 0x00, 0x01, 'o', 'n', 'e',                             // ONE
    0x04, 0x00, 0x02, 't', 'w', 'o',                             // TWO
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    // padding to 20
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // padding to 30
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // padding to 40
};

// TODO: Revisit LTV parsing.  Right now, the padding bytes are parsed
// DEFINE_AND_INSTANTIATE_OneLengthTypeValueStructPaddedReflectionTest(one_length_type_value_struct_padded_20,
// one_length_type_value_struct_padded_28);

TEST(GeneratedPacketTest, testOneLengthTypeValueStructPaddedGeneration) {
  std::vector<LengthTypeValueStruct> ltv_vector;
  LengthTypeValueStruct ltv;
  ltv.type_ = DataType::ONE;
  ltv.value_ = {
      'o',
      'n',
      'e',
  };
  ltv_vector.push_back(ltv);
  ltv.type_ = DataType::TWO;
  ltv.value_ = {
      't',
      'w',
      'o',
  };
  ltv_vector.push_back(ltv);

  auto packet = OneLengthTypeValueStructPaddedBuilder::Create(ltv_vector);
  ASSERT_EQ(one_length_type_value_struct_padded_28.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(one_length_type_value_struct_padded_28.size(), packet_bytes->size());
  for (size_t i = 0; i < one_length_type_value_struct_padded_28.size(); i++) {
    ASSERT_EQ(one_length_type_value_struct_padded_28[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = OneLengthTypeValueStructPaddedView::Create(SizedParentView::Create(packet_bytes_view));
  ASSERT_TRUE(view.IsValid());
  auto an_array = view.GetOneArray();
  // TODO: Revisit LTV parsing.  Right now, the padding bytes are parsed
  // ASSERT_EQ(ltv_vector.size(), an_array.size());
  for (size_t i = 0; i < ltv_vector.size(); i++) {
    ASSERT_EQ(ltv_vector[i].type_, an_array[i].type_);
    ASSERT_EQ(ltv_vector[i].value_, an_array[i].value_);
  }
}

vector<uint8_t> byte_sized{
    0x11,                                            // 1
    0x21, 0x22,                                      // 2
    0x31, 0x32, 0x33,                                // 3
    0x41, 0x42, 0x43, 0x44,                          // 4
    0x51, 0x52, 0x53, 0x54, 0x55,                    // 5
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66,              // 6
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,        // 7
    0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,  // 8
};

TEST(GeneratedPacketTest, testByteSizedFields) {
  uint64_t array[9]{
      0xbadbadbad,
      0x11,                // 1
      0x2221,              // 2
      0x333231,            // 3
      0x44434241,          // 4
      0x5554535251,        // 5
      0x666564636261,      // 6
      0x77767574737271,    // 7
      0x8887868584838281,  // 8
  };
  auto packet =
      ByteSizedFieldsBuilder::Create(array[1], array[2], array[3], array[4], array[5], array[6], array[7], array[8]);
  ASSERT_EQ(byte_sized.size(), packet->size());

  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  packet->Serialize(it);

  ASSERT_EQ(byte_sized.size(), packet_bytes->size());
  for (size_t i = 0; i < byte_sized.size(); i++) {
    ASSERT_EQ(byte_sized[i], packet_bytes->at(i));
  }

  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto view = ByteSizedFieldsView::Create(packet_bytes_view);
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(array[1], view.GetOne());
  ASSERT_EQ(array[2], view.GetTwo());
  ASSERT_EQ(array[3], view.GetThree());
  ASSERT_EQ(array[4], view.GetFour());
  ASSERT_EQ(array[5], view.GetFive());
  ASSERT_EQ(array[6], view.GetSix());
  ASSERT_EQ(array[7], view.GetSeven());
  ASSERT_EQ(array[8], view.GetEight());
}

DEFINE_AND_INSTANTIATE_ByteSizedFieldsReflectionTest(byte_sized);

TEST(GeneratedPacketTest, testOneGenericStructArrayNoZeroEmpty) {
  auto too_few_bytes = std::make_shared<std::vector<uint8_t>>(0);
  auto view = OneGenericStructArrayNoZeroView::Create(too_few_bytes);
  for (size_t i = 0; i < 10; i++) {
    if (view.IsValid()) {
      view.GetAnArray().size();
    }
    too_few_bytes->push_back(0);
    view = OneGenericStructArrayNoZeroView::Create(too_few_bytes);
  }

  std::vector<uint8_t> a_two_byte_struct = {
      static_cast<uint8_t>(StructTypeNoZero::TWO_BYTE),
      0x01,
      0x02,
  };
  too_few_bytes = std::make_shared<std::vector<uint8_t>>(a_two_byte_struct);
  view = OneGenericStructArrayNoZeroView::Create(too_few_bytes);
  ASSERT(view.IsValid());
  ASSERT_EQ(1, view.GetAnArray().size());
}

}  // namespace parser
}  // namespace packet
}  // namespace bluetooth
