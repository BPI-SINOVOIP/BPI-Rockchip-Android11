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

#include "utils/strings/append.h"

#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace strings {

TEST(StringUtilTest, SStringAppendF) {
  std::string str;
  SStringAppendF(&str, 5, "%d %d", 0, 1);
  EXPECT_EQ(str, "0 1");

  SStringAppendF(&str, 1, "%d", 9);
  EXPECT_EQ(str, "0 19");

  SStringAppendF(&str, 1, "%d", 10);
  EXPECT_EQ(str, "0 191");

  str.clear();

  SStringAppendF(&str, 5, "%d", 100);
  EXPECT_EQ(str, "100");
}

TEST(StringUtilTest, SStringAppendFBufCalc) {
  std::string str;
  SStringAppendF(&str, 0, "%d %s %d", 1, "hello", 2);
  EXPECT_EQ(str, "1 hello 2");
}

TEST(StringUtilTest, JoinStrings) {
  std::vector<std::string> vec;
  vec.push_back("1");
  vec.push_back("2");
  vec.push_back("3");

  EXPECT_EQ("1,2,3", JoinStrings(",", vec));
  EXPECT_EQ("123", JoinStrings("", vec));
  EXPECT_EQ("1, 2, 3", JoinStrings(", ", vec));
  EXPECT_EQ("", JoinStrings(",", std::vector<std::string>()));
}

}  // namespace strings
}  // namespace libtextclassifier3
