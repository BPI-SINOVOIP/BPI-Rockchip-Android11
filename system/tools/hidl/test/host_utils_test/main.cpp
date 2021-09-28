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

#define LOG_TAG "libhidl-gen-host-utils"

#include <hidl-util/StringHelper.h>

#include <gtest/gtest.h>
#include <vector>

using ::android::StringHelper;

class LibHidlGenUtilsTest : public ::testing::Test {};

TEST_F(LibHidlGenUtilsTest, EndsWithTest) {
    EXPECT_TRUE(StringHelper::EndsWith("", ""));
    EXPECT_TRUE(StringHelper::EndsWith("a", ""));
    EXPECT_TRUE(StringHelper::EndsWith("b", "b"));
    EXPECT_TRUE(StringHelper::EndsWith("ab", "b"));
    EXPECT_TRUE(StringHelper::EndsWith("ab", "ab"));
    EXPECT_TRUE(StringHelper::EndsWith("abcd", "bcd"));
    EXPECT_TRUE(StringHelper::EndsWith("abcd", "abcd"));
    EXPECT_TRUE(StringHelper::EndsWith("abcdefghijk", "ijk"));
    EXPECT_TRUE(StringHelper::EndsWith("abcdefghijk", "bcdefghijk"));

    EXPECT_FALSE(StringHelper::EndsWith("", "a"));
    EXPECT_FALSE(StringHelper::EndsWith("b", "a"));
    EXPECT_FALSE(StringHelper::EndsWith("abcd", "ab"));
}

TEST_F(LibHidlGenUtilsTest, StartsWithTest) {
    EXPECT_TRUE(StringHelper::StartsWith("", ""));
    EXPECT_TRUE(StringHelper::StartsWith("a", ""));
    EXPECT_TRUE(StringHelper::StartsWith("b", "b"));
    EXPECT_TRUE(StringHelper::StartsWith("ab", "a"));
    EXPECT_TRUE(StringHelper::StartsWith("ab", "ab"));
    EXPECT_TRUE(StringHelper::StartsWith("abcd", "abc"));
    EXPECT_TRUE(StringHelper::StartsWith("abcd", "abcd"));
    EXPECT_TRUE(StringHelper::StartsWith("abcdefghijk", "abc"));
    EXPECT_TRUE(StringHelper::StartsWith("abcdefghijk", "abcdefghij"));

    EXPECT_FALSE(StringHelper::StartsWith("", "a"));
    EXPECT_FALSE(StringHelper::StartsWith("b", "a"));
    EXPECT_FALSE(StringHelper::StartsWith("abcd", "cd"));
}

TEST_F(LibHidlGenUtilsTest, Trim) {
    EXPECT_EQ("", StringHelper::LTrim("", ""));
    EXPECT_EQ("", StringHelper::LTrim("", "a"));
    EXPECT_EQ("", StringHelper::LTrim("a", "a"));
    EXPECT_EQ("a", StringHelper::LTrim("a", ""));
    EXPECT_EQ("a", StringHelper::LTrim("a", "b"));
    EXPECT_EQ("a", StringHelper::LTrim("ba", "b"));
    EXPECT_EQ("f", StringHelper::LTrim("abcdef", "abcde"));
    EXPECT_EQ("cdef", StringHelper::LTrim("abcdef", "ab"));
    EXPECT_EQ("abcdef", StringHelper::LTrim("abcdef", ""));

    EXPECT_EQ("", StringHelper::RTrim("", ""));
    EXPECT_EQ("", StringHelper::RTrim("", "a"));
    EXPECT_EQ("", StringHelper::RTrim("a", "a"));
    EXPECT_EQ("a", StringHelper::RTrim("a", ""));
    EXPECT_EQ("a", StringHelper::RTrim("a", "b"));
    EXPECT_EQ("a", StringHelper::RTrim("ab", "b"));
    EXPECT_EQ("a", StringHelper::RTrim("abcdef", "bcdef"));
    EXPECT_EQ("abcd", StringHelper::RTrim("abcdef", "ef"));
    EXPECT_EQ("abcdef", StringHelper::RTrim("abcdef", ""));
}

TEST_F(LibHidlGenUtilsTest, TrimAll) {
    EXPECT_EQ("", StringHelper::LTrimAll("", ""));
    EXPECT_EQ("", StringHelper::LTrimAll("", "a"));
    EXPECT_EQ("", StringHelper::LTrimAll("", "ab"));
    EXPECT_EQ("", StringHelper::LTrimAll("a", "a"));
    EXPECT_EQ("", StringHelper::LTrimAll("aa", "a"));
    EXPECT_EQ("b", StringHelper::LTrimAll("b", "a"));
    EXPECT_EQ("b", StringHelper::LTrimAll("aaab", "a"));
    EXPECT_EQ("c", StringHelper::LTrimAll("ababc", "ab"));
    EXPECT_EQ("ac", StringHelper::LTrimAll("abac", "ab"));

    EXPECT_EQ("", StringHelper::RTrimAll("", ""));
    EXPECT_EQ("", StringHelper::RTrimAll("", "a"));
    EXPECT_EQ("", StringHelper::RTrimAll("", "ab"));
    EXPECT_EQ("", StringHelper::RTrimAll("a", "a"));
    EXPECT_EQ("", StringHelper::RTrimAll("aa", "a"));
    EXPECT_EQ("b", StringHelper::RTrimAll("b", "a"));
    EXPECT_EQ("b", StringHelper::RTrimAll("baaa", "a"));
    EXPECT_EQ("c", StringHelper::RTrimAll("cabab", "ab"));
    EXPECT_EQ("ca", StringHelper::RTrimAll("caba", "ba"));
}

TEST_F(LibHidlGenUtilsTest, SplitString) {
    std::vector<std::string> components;

    StringHelper::SplitString("", '.', &components);
    EXPECT_EQ(std::vector<std::string>({""}), components);
    StringHelper::SplitString("a.", '.', &components);
    EXPECT_EQ(std::vector<std::string>({"a", ""}), components);
    StringHelper::SplitString(".a", '.', &components);
    EXPECT_EQ(std::vector<std::string>({"", "a"}), components);
    StringHelper::SplitString("..", '.', &components);
    EXPECT_EQ(std::vector<std::string>({"", "", ""}), components);
    StringHelper::SplitString("asdf.asdf", '.', &components);
    EXPECT_EQ(std::vector<std::string>({"asdf", "asdf"}), components);
}

TEST_F(LibHidlGenUtilsTest, JoinStrings) {
    EXPECT_EQ("", StringHelper::JoinStrings({}, ""));
    EXPECT_EQ("", StringHelper::JoinStrings({}, "a"));
    EXPECT_EQ("a", StringHelper::JoinStrings({"a"}, ""));
    EXPECT_EQ("a,b", StringHelper::JoinStrings({"a", "b"}, ","));
    EXPECT_EQ("ab,", StringHelper::JoinStrings({"ab", ""}, ","));
    EXPECT_EQ(",ab", StringHelper::JoinStrings({"", "ab"}, ","));
    EXPECT_EQ("a.,b", StringHelper::JoinStrings({"a", "b"}, ".,"));
    EXPECT_EQ("a,b,c", StringHelper::JoinStrings({"a", "b", "c"}, ","));
    EXPECT_EQ("abc.,def.,ghi", StringHelper::JoinStrings({"abc", "def", "ghi"}, ".,"));
}

// This test checks if Tokenize is working correctly. Since that is private, this just converts
// everything to upper snake case.
TEST_F(LibHidlGenUtilsTest, TokenizeTest) {
    // Basic cases
    EXPECT_EQ("UPPER_SNAKE_CASE", StringHelper::ToUpperSnakeCase("UPPER_SNAKE_CASE"));
    EXPECT_EQ("UPPER_SNAKE_CASE", StringHelper::ToUpperSnakeCase("upper_snake_case"));
    EXPECT_EQ("UPPER_SNAKE_CASE", StringHelper::ToUpperSnakeCase("UpperSnakeCase"));
    EXPECT_EQ("UPPER_SNAKE_CASE", StringHelper::ToUpperSnakeCase("upperSnakeCase"));

    // Cases with numbers
    // TODO: The following case "Some3DText" should be tokenized as SOME_3D_TEXT
    // EXPECT_EQ("SOME3_D_TEXT", StringHelper::ToUpperSnakeCase("Some3DText"));
    EXPECT_EQ("SOME_3D_TEXT", StringHelper::ToUpperSnakeCase("Some_3D_Text"));
    EXPECT_EQ("IPV4_ADDR", StringHelper::ToUpperSnakeCase("Ipv4Addr"));
    EXPECT_EQ("IPV4_ADDR", StringHelper::ToUpperSnakeCase("Ipv4_Addr"));

    // Cases with incorrect PascalCase
    EXPECT_EQ("UI_ERROR", StringHelper::ToUpperSnakeCase("UIError"));
    EXPECT_EQ("UI_ERROR", StringHelper::ToUpperSnakeCase("UI_Error"));
    EXPECT_EQ("PROTOCOL_B_PRIME", StringHelper::ToUpperSnakeCase("protocolBPrime"));
    EXPECT_EQ("PROTOCOL_B_PRIME", StringHelper::ToUpperSnakeCase("protocol_B_Prime"));

    // Fully uppercase
    EXPECT_EQ("VAL2", StringHelper::ToUpperSnakeCase("VAL2"));
    EXPECT_EQ("VAL2OTHER", StringHelper::ToUpperSnakeCase("VAL2OTHER"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
