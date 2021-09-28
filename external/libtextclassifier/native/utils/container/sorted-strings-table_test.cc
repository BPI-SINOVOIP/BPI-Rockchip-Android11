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

#include "utils/container/sorted-strings-table.h"

#include <vector>

#include "utils/base/integral_types.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(SortedStringsTest, Lookup) {
  const char pieces[] = "hell\0hello\0o\0there\0";
  const uint32 offsets[] = {0, 5, 11, 13};

  SortedStringsTable table(/*num_pieces=*/4, offsets, StringPiece(pieces, 18),
                           /*use_linear_scan_threshold=*/1);

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(table.FindAllPrefixMatches("hello there", &matches));
    EXPECT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0].id, 0 /*hell*/);
    EXPECT_EQ(matches[0].match_length, 4 /*hell*/);
    EXPECT_EQ(matches[1].id, 1 /*hello*/);
    EXPECT_EQ(matches[1].match_length, 5 /*hello*/);
  }

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(table.FindAllPrefixMatches("he", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(table.FindAllPrefixMatches("he", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(table.FindAllPrefixMatches("abcd", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(table.FindAllPrefixMatches("", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(table.FindAllPrefixMatches("hi there", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(table.FindAllPrefixMatches(StringPiece("\0", 1), &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<StringSet::Match> matches;
    EXPECT_TRUE(
        table.FindAllPrefixMatches(StringPiece("\xff, \xfe", 2), &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    StringSet::Match match;
    EXPECT_TRUE(table.LongestPrefixMatch("hella there", &match));
    EXPECT_EQ(match.id, 0 /*hell*/);
  }

  {
    StringSet::Match match;
    EXPECT_TRUE(table.LongestPrefixMatch("hello there", &match));
    EXPECT_EQ(match.id, 1 /*hello*/);
  }

  {
    StringSet::Match match;
    EXPECT_TRUE(table.LongestPrefixMatch("abcd", &match));
    EXPECT_EQ(match.id, -1);
  }

  {
    StringSet::Match match;
    EXPECT_TRUE(table.LongestPrefixMatch("", &match));
    EXPECT_EQ(match.id, -1);
  }

  {
    int value;
    EXPECT_TRUE(table.Find("hell", &value));
    EXPECT_EQ(value, 0);
  }

  {
    int value;
    EXPECT_FALSE(table.Find("hella", &value));
  }

  {
    int value;
    EXPECT_TRUE(table.Find("hello", &value));
    EXPECT_EQ(value, 1 /*hello*/);
  }
}

}  // namespace
}  // namespace libtextclassifier3
