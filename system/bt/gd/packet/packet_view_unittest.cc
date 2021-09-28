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

#include "packet/packet_view.h"

#include <gtest/gtest.h>
#include <forward_list>
#include <memory>

#include "hci/address.h"

using bluetooth::hci::Address;
using bluetooth::packet::PacketView;
using bluetooth::packet::View;
using std::vector;

namespace {
vector<uint8_t> count_all = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};

vector<uint8_t> count_1 = {
    0x00,
    0x01,
    0x02,
};

vector<uint8_t> count_2 = {
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
};

vector<uint8_t> count_3 = {
    0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};
}  // namespace

namespace bluetooth {
namespace packet {

template <typename T>
class IteratorTest : public ::testing::Test {
 public:
  IteratorTest() = default;
  ~IteratorTest() = default;

  void SetUp() {
    packet = std::shared_ptr<T>(new T({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())}));
  }

  void TearDown() {
    packet.reset();
  }

  std::shared_ptr<T> packet;
};

using PacketViewTypes = ::testing::Types<PacketView<true>, PacketView<false>>;
TYPED_TEST_CASE(IteratorTest, PacketViewTypes);

class IteratorExtractTest : public ::testing::Test {
 public:
  IteratorExtractTest() = default;
  ~IteratorExtractTest() = default;
};

template <typename T>
class PacketViewTest : public IteratorTest<T> {
 public:
  PacketViewTest() = default;
  ~PacketViewTest() = default;
};

using PacketViewTypes = ::testing::Types<PacketView<true>, PacketView<false>>;
TYPED_TEST_CASE(PacketViewTest, PacketViewTypes);

class PacketViewMultiViewTest : public ::testing::Test {
 public:
  PacketViewMultiViewTest() = default;
  ~PacketViewMultiViewTest() = default;

  const PacketView<true> single_view =
      PacketView<true>({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  const PacketView<true> multi_view = PacketView<true>({
      View(std::make_shared<const vector<uint8_t>>(count_1), 0, count_1.size()),
      View(std::make_shared<const vector<uint8_t>>(count_2), 0, count_2.size()),
      View(std::make_shared<const vector<uint8_t>>(count_3), 0, count_3.size()),
  });
};

class PacketViewMultiViewAppendTest : public ::testing::Test {
 public:
  PacketViewMultiViewAppendTest() = default;
  ~PacketViewMultiViewAppendTest() = default;

  class AppendedPacketView : public PacketView<true> {
   public:
    AppendedPacketView(PacketView<true> first, std::forward_list<PacketView<true>> to_append)
        : PacketView<true>(first) {
      for (const auto& packet_view : to_append) {
        Append(packet_view);
      }
    }
  };
  const PacketView<true> single_view =
      PacketView<true>({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  const PacketView<true> multi_view = AppendedPacketView(
      PacketView<true>({View(std::make_shared<const vector<uint8_t>>(count_1), 0, count_1.size())}),
      {PacketView<true>({View(std::make_shared<const vector<uint8_t>>(count_2), 0, count_2.size())}),
       PacketView<true>({View(std::make_shared<const vector<uint8_t>>(count_3), 0, count_3.size())})});
};

class ViewTest : public ::testing::Test {
 public:
  ViewTest() = default;
  ~ViewTest() = default;
};

TEST(IteratorExtractTest, extractLeTest) {
  PacketView<true> packet({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  auto general_case = packet.begin();

  ASSERT_EQ(0x00, general_case.extract<uint8_t>());
  ASSERT_EQ(0x0201, general_case.extract<uint16_t>());
  ASSERT_EQ(0x06050403u, general_case.extract<uint32_t>());
  ASSERT_EQ(0x0e0d0c0b0a090807u, general_case.extract<uint64_t>());
  ASSERT_EQ(0x0f, general_case.extract<uint8_t>());
  Address raw({0x10, 0x11, 0x12, 0x13, 0x14, 0x15});
  ASSERT_EQ(raw, general_case.extract<Address>());
  ASSERT_EQ(0x16, general_case.extract<uint8_t>());
}

TEST(IteratorExtractTest, extractBeTest) {
  PacketView<false> packet({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  auto general_case = packet.begin();

  ASSERT_EQ(0x00, general_case.extract<uint8_t>());
  ASSERT_EQ(0x0102, general_case.extract<uint16_t>());
  ASSERT_EQ(0x03040506u, general_case.extract<uint32_t>());
  ASSERT_EQ(0x0708090a0b0c0d0eu, general_case.extract<uint64_t>());
  ASSERT_EQ(0x0f, general_case.extract<uint8_t>());
  Address raw({0x15, 0x14, 0x13, 0x12, 0x11, 0x10});
  ASSERT_EQ(raw, general_case.extract<Address>());
  ASSERT_EQ(0x16, general_case.extract<uint8_t>());
}

TYPED_TEST(IteratorTest, extractBoundsDeathTest) {
  auto bounds_test = this->packet->end();

  ASSERT_DEATH(bounds_test.template extract<uint8_t>(), "");
  ASSERT_DEATH(bounds_test.template extract<uint16_t>(), "");
  ASSERT_DEATH(bounds_test.template extract<uint32_t>(), "");
  ASSERT_DEATH(bounds_test.template extract<uint64_t>(), "");
}

TYPED_TEST(IteratorTest, dereferenceDeathTest) {
  auto dereference_test = this->packet->end();

  ASSERT_DEATH(*dereference_test, "");
  ASSERT_EQ(0x1f, *(dereference_test - 1));
}

TYPED_TEST(IteratorTest, plusEqTest) {
  auto plus_eq = this->packet->begin();
  for (size_t i = 0; i < count_all.size(); i += 2) {
    ASSERT_EQ(count_all[i], *plus_eq) << "+= test: Dereferenced iterator does not equal expected at index " << i;
    plus_eq += 2;
  }
}

TYPED_TEST(IteratorTest, preIncrementTest) {
  auto plus_plus = this->packet->begin();
  for (size_t i = 0; i < count_all.size() - 1; i++) {
    ASSERT_EQ(count_all[i + 1], *(++plus_plus)) << "Pre-increment test: Dereferenced iterator does not equal expected "
                                                << "at index " << i;
  }
}

TYPED_TEST(IteratorTest, postIncrementTest) {
  auto plus_plus = this->packet->begin();
  for (size_t i = 0; i < count_all.size(); i++) {
    ASSERT_EQ(count_all[i], *(plus_plus++)) << "Post-increment test: Dereferenced iterator does not equal expected "
                                            << "at index " << i;
  }
}

TYPED_TEST(IteratorTest, additionTest) {
  auto plus = this->packet->begin();
  for (size_t i = 0; i < count_all.size(); i++) {
    ASSERT_EQ(count_all[i], *plus) << "+ test: Dereferenced iterator does not equal expected at index " << i;
    plus = plus + 1;
  }
}

TYPED_TEST(IteratorTest, minusEqTest) {
  auto minus_eq = this->packet->end();
  minus_eq -= 1;
  size_t index = count_all.size() - 1;
  for (size_t i = 0; index > i; i++) {
    ASSERT_EQ(count_all[index], *minus_eq)
        << "-= test: Dereferenced iterator does not equal expected at index " << index;
    index -= i;
    minus_eq -= i;
  }
}

TYPED_TEST(IteratorTest, preDecrementTest) {
  auto minus_minus = this->packet->end();
  for (size_t i = count_all.size(); i > 0; i--) {
    ASSERT_EQ(count_all[i - 1], *(--minus_minus))
        << "Pre-decrement test: Dereferenced iterator does not equal expected "
        << "at index " << i;
  }
}

TYPED_TEST(IteratorTest, postDecrementTest) {
  auto minus_minus = this->packet->end();
  minus_minus--;
  for (size_t i = count_all.size() - 1; i > 0; i--) {
    ASSERT_EQ(count_all[i], *(minus_minus--)) << "Post-decrement test: Dereferenced iterator does not equal expected "
                                              << "at index " << i;
  }
}

TYPED_TEST(IteratorTest, subtractionTest) {
  auto minus = this->packet->end();
  minus = minus - 1;
  for (size_t i = count_all.size() - 1; i > 0; i--) {
    ASSERT_EQ(count_all[i], *minus) << "- test: Dereferenced iterator does not equal expected at index " << i;
    minus = minus - 1;
  }
}

TYPED_TEST(IteratorTest, differenceTest) {
  auto begin = this->packet->begin();
  auto end = this->packet->end();
  int difference = end - begin;
  ASSERT_EQ(difference, static_cast<int>(count_all.size()));
  int neg_difference = begin - end;
  ASSERT_EQ(neg_difference, -static_cast<int>(count_all.size()));
}

TYPED_TEST(IteratorTest, equalityTest) {
  auto begin = this->packet->begin();
  auto end = this->packet->end();
  auto begin_copy = this->packet->begin();
  auto end_copy = this->packet->end();
  ASSERT_EQ(begin_copy, begin);
  ASSERT_EQ(end_copy, end);
}

TYPED_TEST(IteratorTest, comparisonsTest) {
  auto begin = this->packet->begin();
  auto end = this->packet->end();
  auto begin_copy = this->packet->begin();
  auto end_copy = this->packet->end();
  ASSERT_EQ(begin_copy, begin);
  ASSERT_EQ(end_copy, end);
  ASSERT_NE(begin, end);
  ASSERT_TRUE(begin < end);
  ASSERT_FALSE(end < end);
  ASSERT_FALSE(end < begin);
  ASSERT_FALSE(begin > end);
  ASSERT_FALSE(end > end);
  ASSERT_TRUE(end > begin);
  ASSERT_TRUE(begin <= end);
  ASSERT_TRUE(end <= end);
  ASSERT_FALSE(end <= begin);
  ASSERT_FALSE(begin >= end);
  ASSERT_TRUE(end >= end);
  ASSERT_TRUE(end >= begin);
}

TYPED_TEST(PacketViewTest, getLengthTest) {
  size_t length = this->packet->size();
  ASSERT_EQ(length, count_all.size());
}

TYPED_TEST(PacketViewTest, getAtIndexTest) {
  size_t past_end = this->packet->size();
  ASSERT_DEATH(this->packet->at(past_end), "");
  size_t working_index = 0x1f;
  ASSERT_EQ(0x1f, this->packet->at(working_index));
}

TYPED_TEST(PacketViewTest, arrayOperatorTest) {
  size_t past_end = this->packet->size();
  ASSERT_DEATH((*(this->packet))[past_end], "");
  size_t working_index = 0x1f;
  ASSERT_EQ(0x1f, (*(this->packet))[working_index]);
}

TYPED_TEST(IteratorTest, numBytesRemainingTest) {
  auto all = this->packet->begin();
  size_t remaining = all.NumBytesRemaining();
  for (size_t n = remaining; n > 0; n--) {
    ASSERT_EQ(remaining, all.NumBytesRemaining());
    all++;
    remaining--;
  }
  ASSERT_EQ(static_cast<size_t>(0), all.NumBytesRemaining());
  ASSERT_DEATH(*(all++), "");
  all++;
  ASSERT_EQ(static_cast<size_t>(0), all.NumBytesRemaining());
  ASSERT_DEATH(*(all++), "");
}

TYPED_TEST(IteratorTest, subrangeTest) {
  auto empty = this->packet->begin().Subrange(0, 0);
  ASSERT_EQ(static_cast<size_t>(0), empty.NumBytesRemaining());
  ASSERT_DEATH(*empty, "");

  empty = this->packet->begin().Subrange(this->packet->size(), 1);
  ASSERT_EQ(static_cast<size_t>(0), empty.NumBytesRemaining());
  ASSERT_DEATH(*empty, "");

  auto all = this->packet->begin();
  auto fullrange = all.Subrange(0, all.NumBytesRemaining());
  ASSERT_EQ(all.NumBytesRemaining(), fullrange.NumBytesRemaining());
  ASSERT_EQ(*(all + 1), 1);

  fullrange = all.Subrange(0, all.NumBytesRemaining() + 1);
  ASSERT_EQ(all.NumBytesRemaining(), fullrange.NumBytesRemaining());
  ASSERT_EQ(*(all + 1), 1);

  fullrange = all.Subrange(0, all.NumBytesRemaining() + 10);
  ASSERT_EQ(all.NumBytesRemaining(), fullrange.NumBytesRemaining());
  ASSERT_EQ(*(all + 1), 1);

  auto subrange = all.Subrange(0, 1);
  ASSERT_EQ(1, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange), 0);

  subrange = this->packet->begin().Subrange(0, 4);
  ASSERT_EQ(4, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange + 1), 1);

  subrange = all.Subrange(0, 3);
  ASSERT_EQ(3, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange + 1), 1);

  subrange = all.Subrange(0, all.NumBytesRemaining() - 1);
  ASSERT_EQ(all.NumBytesRemaining() - 1, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange + 1), 1);

  subrange = all.Subrange(0, all.NumBytesRemaining() - 2);
  ASSERT_EQ(all.NumBytesRemaining() - 2, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange + 1), 1);

  subrange = all.Subrange(1, all.NumBytesRemaining());
  ASSERT_EQ(all.NumBytesRemaining() - 1, subrange.NumBytesRemaining());
  ASSERT_EQ(*subrange, 1);

  subrange = all.Subrange(2, all.NumBytesRemaining());
  ASSERT_EQ(all.NumBytesRemaining() - 2, subrange.NumBytesRemaining());
  ASSERT_EQ(*subrange, 2);

  subrange = all.Subrange(1, all.NumBytesRemaining() - 1);
  ASSERT_EQ(all.NumBytesRemaining() - 1, subrange.NumBytesRemaining());
  ASSERT_EQ(*subrange, 1);

  subrange = all.Subrange(2, all.NumBytesRemaining() - 2);
  ASSERT_EQ(all.NumBytesRemaining() - 2, subrange.NumBytesRemaining());
  ASSERT_EQ(*subrange, 2);

  subrange = all.Subrange(1, 1);
  ASSERT_EQ(1, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange), 1);

  subrange = all.Subrange(1, 2);
  ASSERT_EQ(2, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange), 1);

  subrange = all.Subrange(2, 1);
  ASSERT_EQ(1, subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange), 2);

  subrange = this->packet->begin().Subrange(this->packet->size() - 1, 2);
  ASSERT_EQ(static_cast<size_t>(1), subrange.NumBytesRemaining());
  ASSERT_EQ(*(subrange), this->packet->size() - 1);
}

using SubviewTestParam = std::pair<size_t, size_t>;
class SubviewBaseTest : public ::testing::TestWithParam<SubviewTestParam> {
 public:
  class SubPacketView : public PacketView<true> {
   public:
    using PacketView<true>::PacketView;
    PacketView<true> Slice(size_t header, size_t tail) {
      return PacketView<true>::GetLittleEndianSubview(header, tail);
    }
  };
};

class SubviewPassTest : public SubviewBaseTest {};

TEST_P(SubviewPassTest, subviewTest) {
  auto header = GetParam().first;
  auto tail = GetParam().second;
  SubPacketView single_view({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  SubPacketView multi_view({
      View(std::make_shared<const vector<uint8_t>>(count_1), 0, count_1.size()),
      View(std::make_shared<const vector<uint8_t>>(count_2), 0, count_2.size()),
      View(std::make_shared<const vector<uint8_t>>(count_3), 0, count_3.size()),
  });

  auto single_slice = single_view.Slice(header, tail);
  auto multi_slice = multi_view.Slice(header, tail);

  ASSERT_EQ(single_slice.size(), tail - header);
  ASSERT_EQ(single_slice.size(), multi_slice.size());
  for (size_t i = 0; i < single_slice.size(); i++) {
    ASSERT_EQ(single_slice[i], multi_slice[i]);
  }
}

static const size_t boundary_1 = count_1.size();
static const size_t boundary_2 = count_1.size() + count_2.size();

INSTANTIATE_TEST_CASE_P(
    chopomatic, SubviewPassTest,
    ::testing::Values(
        // {begin, end} pairs for subsets into the PacketView
        SubviewTestParam{0, 0}, SubviewTestParam{0, boundary_1}, SubviewTestParam{0, boundary_1 + 1},
        SubviewTestParam{0, boundary_2}, SubviewTestParam{0, boundary_2 + 1}, SubviewTestParam{0, count_all.size()},
        SubviewTestParam{boundary_1 - 1, boundary_1}, SubviewTestParam{boundary_1 - 1, boundary_1 + 1},
        SubviewTestParam{boundary_1 - 1, boundary_2}, SubviewTestParam{boundary_1 - 1, boundary_2 + 1},
        SubviewTestParam{boundary_1 - 1, count_all.size()}, SubviewTestParam{boundary_1, boundary_1},
        SubviewTestParam{boundary_1, boundary_2}, SubviewTestParam{boundary_1, boundary_2 + 1},
        SubviewTestParam{boundary_1, count_all.size()}, SubviewTestParam{boundary_2 - 1, boundary_2},
        SubviewTestParam{boundary_2 - 1, boundary_2 + 1}, SubviewTestParam{boundary_2 - 1, count_all.size()},
        SubviewTestParam{boundary_2, boundary_2}, SubviewTestParam{boundary_2, boundary_2 + 1},
        SubviewTestParam{boundary_2, count_all.size()}, SubviewTestParam{count_all.size() - 1, count_all.size()},
        SubviewTestParam{count_all.size(), count_all.size()}));

class SubviewDeathTest : public SubviewBaseTest {};

TEST_P(SubviewDeathTest, subviewDeathTest) {
  auto header = GetParam().first;
  auto tail = GetParam().second;
  SubPacketView single_view({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  SubPacketView multi_view({
      View(std::make_shared<const vector<uint8_t>>(count_1), 0, count_1.size()),
      View(std::make_shared<const vector<uint8_t>>(count_2), 0, count_2.size()),
      View(std::make_shared<const vector<uint8_t>>(count_3), 0, count_3.size()),
  });

  ASSERT_DEATH(auto single_slice = single_view.Slice(header, tail), "");
  ASSERT_DEATH(auto multi_slice = multi_view.Slice(header, tail), "");
}

INSTANTIATE_TEST_CASE_P(chopomaticDeath, SubviewDeathTest,
                        ::testing::Values(
                            // {begin, end} pairs for subsets into the PacketView
                            SubviewTestParam{1, 0}, SubviewTestParam{count_all.size(), count_all.size() - 1},
                            SubviewTestParam{count_all.size(), count_all.size() + 1}));

TEST(SubviewTest, simpleSubviewTest) {
  PacketView<true> view({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  PacketView<true> sub_1_view = view.GetLittleEndianSubview(0, view.size());
  PacketView<true> sub_2_view = sub_1_view.GetLittleEndianSubview(0, sub_1_view.size());
  PacketView<true> sub_3_view = sub_2_view.GetLittleEndianSubview(0, sub_2_view.size());
  PacketView<true> sub_4_view = sub_3_view.GetLittleEndianSubview(0, sub_3_view.size());
  ASSERT_EQ(sub_1_view.size(), view.size());
  ASSERT_EQ(sub_2_view.size(), view.size());
  ASSERT_EQ(sub_3_view.size(), view.size());
  ASSERT_EQ(sub_4_view.size(), view.size());
}

TEST(SubviewTest, realSubviewTest) {
  PacketView<true> view({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  std::vector<PacketView<true>> sub_views{view};
  for (size_t i = 1; i < 6; i++) {
    size_t parent_size = sub_views[i - 1].size();
    sub_views.push_back(sub_views[i - 1].GetLittleEndianSubview(1, parent_size - 1));
    ASSERT_EQ(sub_views[i][0], i);
    ASSERT_EQ(sub_views[i].size(), parent_size - 2);
  }
}

TEST(SubviewTest, subSubviewTest) {
  PacketView<true> single_view({View(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size())});
  PacketView<true> multi_view({
      View(std::make_shared<const vector<uint8_t>>(count_1), 0, count_1.size()),
      View(std::make_shared<const vector<uint8_t>>(count_2), 0, count_2.size()),
      View(std::make_shared<const vector<uint8_t>>(count_3), 0, count_3.size()),
  });
  ASSERT_EQ(single_view.size(), multi_view.size());
  for (size_t i = 0; i < count_all.size() / 2; i++) {
    PacketView<true> sub_single_view = single_view.GetLittleEndianSubview(i, count_all.size() - i);
    PacketView<true> sub_multi_view = multi_view.GetLittleEndianSubview(i, count_all.size() - i);
    ASSERT_EQ(count_all.size() - 2 * i, sub_single_view.size());
    ASSERT_EQ(sub_single_view.size(), sub_multi_view.size());
    for (size_t j = 0; j < sub_single_view.size() / 2; j++) {
      PacketView<true> sub_sub_single_view = sub_single_view.GetLittleEndianSubview(j, sub_single_view.size() - j);
      PacketView<true> sub_sub_multi_view = sub_multi_view.GetLittleEndianSubview(j, sub_multi_view.size() - j);
      ASSERT_EQ(sub_single_view.size() - 2 * j, sub_sub_single_view.size());
      ASSERT_EQ(sub_sub_single_view.size(), sub_sub_multi_view.size());
    }
  }
}

TEST_F(PacketViewMultiViewTest, sizeTest) {
  ASSERT_EQ(single_view.size(), multi_view.size());
}

TEST_F(PacketViewMultiViewTest, dereferenceTestLittleEndian) {
  auto single_itr = single_view.begin();
  auto multi_itr = multi_view.begin();
  for (size_t i = 0; i < single_view.size(); i++) {
    ASSERT_EQ(*(single_itr++), *(multi_itr++));
  }
  ASSERT_DEATH(*multi_itr, "");
}

TEST_F(PacketViewMultiViewTest, dereferenceTestBigEndian) {
  auto single_itr = single_view.begin();
  auto multi_itr = multi_view.begin();
  for (size_t i = 0; i < single_view.size(); i++) {
    ASSERT_EQ(*(single_itr++), *(multi_itr++));
  }
  ASSERT_DEATH(*multi_itr, "");
}

TEST_F(PacketViewMultiViewTest, arrayOperatorTest) {
  for (size_t i = 0; i < single_view.size(); i++) {
    ASSERT_EQ(single_view[i], multi_view[i]);
  }
  ASSERT_DEATH(multi_view[single_view.size()], "");
}

TEST_F(PacketViewMultiViewAppendTest, sizeTestAppend) {
  ASSERT_EQ(single_view.size(), multi_view.size());
}

TEST_F(PacketViewMultiViewAppendTest, dereferenceTestLittleEndianAppend) {
  auto single_itr = single_view.begin();
  auto multi_itr = multi_view.begin();
  for (size_t i = 0; i < single_view.size(); i++) {
    ASSERT_EQ(*(single_itr++), *(multi_itr++));
  }
  ASSERT_DEATH(*multi_itr, "");
}

TEST_F(PacketViewMultiViewAppendTest, dereferenceTestBigEndianAppend) {
  auto single_itr = single_view.begin();
  auto multi_itr = multi_view.begin();
  for (size_t i = 0; i < single_view.size(); i++) {
    ASSERT_EQ(*(single_itr++), *(multi_itr++));
  }
  ASSERT_DEATH(*multi_itr, "");
}

TEST_F(PacketViewMultiViewAppendTest, arrayOperatorTestAppend) {
  for (size_t i = 0; i < single_view.size(); i++) {
    ASSERT_EQ(single_view[i], multi_view[i]);
  }
  ASSERT_DEATH(multi_view[single_view.size()], "");
}

TEST(ViewTest, arrayOperatorTest) {
  View view_all(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size());
  size_t past_end = view_all.size();
  for (size_t i = 0; i < past_end; i++) {
    ASSERT_EQ(view_all[i], count_all[i]);
  }
  ASSERT_DEATH(view_all[past_end], "");

  size_t header_size = 2;
  size_t tail_size = 3;
  View view_subset(std::make_shared<const vector<uint8_t>>(count_all), header_size, count_all.size() - tail_size);
  View view_subset2(view_all, header_size, count_all.size() - tail_size);
  size_t subset_length = view_subset.size();
  for (size_t i = 0; i < subset_length; i++) {
    ASSERT_EQ(view_subset[i], count_all[header_size + i]);
    ASSERT_EQ(view_subset[i], view_subset2[i]);
  }
  ASSERT_DEATH(view_subset[subset_length + 1], "");
  ASSERT_DEATH(view_subset2[subset_length + 1], "");
}

TEST(ViewTest, earlySubSubviewTest) {
  View view(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size());
  View sub_1_view(view, view.size() - 3, view.size() - 1);
  View sub_2_view(sub_1_view, 1, 2);
  ASSERT_EQ(sub_1_view.size(), 2u);
  ASSERT_EQ(sub_2_view.size(), 1u);
}

TEST(ViewTest, subSubviewTest) {
  View view(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size());
  std::vector<View> sub_views{view};
  for (size_t i = 1; i < 6; i++) {
    size_t parent_size = sub_views[i - 1].size();
    sub_views.push_back({View(sub_views[i - 1], 1, parent_size - 1)});
    ASSERT_EQ(sub_views[i][0], i);
    ASSERT_EQ(sub_views[i].size(), parent_size - 2);
  }
}

TEST(ViewTest, zeroSubviewTest) {
  View view(std::make_shared<const vector<uint8_t>>(count_all), 0, count_all.size());
  View subview(view, view.size(), view.size() + 1);
  ASSERT_EQ(subview.size(), 0u);
}
}  // namespace packet
}  // namespace bluetooth
