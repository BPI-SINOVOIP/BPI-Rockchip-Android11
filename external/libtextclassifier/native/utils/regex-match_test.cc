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

#include "utils/regex-match.h"

#include <memory>

#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

class RegexMatchTest : public testing::Test {
 protected:
  RegexMatchTest() : INIT_UNILIB_FOR_TESTING(unilib_) {}
  UniLib unilib_;
};

#ifdef TC3_UNILIB_ICU
#ifndef TC3_DISABLE_LUA
TEST_F(RegexMatchTest, HandlesSimpleVerification) {
  EXPECT_TRUE(VerifyMatch(/*context=*/"", /*matcher=*/nullptr, "return true;"));
}
#endif  // TC3_DISABLE_LUA

#ifndef TC3_DISABLE_LUA
TEST_F(RegexMatchTest, HandlesCustomVerification) {
  UnicodeText pattern = UTF8ToUnicodeText("(\\d{16})",
                                          /*do_copy=*/true);
  UnicodeText message = UTF8ToUnicodeText("cc: 4012888888881881",
                                          /*do_copy=*/true);
  const std::string verifier = R"(
function luhn(candidate)
    local sum = 0
    local num_digits = string.len(candidate)
    local parity = num_digits % 2
    for pos = 1,num_digits do
      d = tonumber(string.sub(candidate, pos, pos))
      if pos % 2 ~= parity then
        d = d * 2
      end
      if d > 9 then
        d = d - 9
      end
      sum = sum + d
    end
    return (sum % 10) == 0
end
return luhn(match[1].text);
  )";
  const std::unique_ptr<UniLib::RegexPattern> regex_pattern =
      unilib_.CreateRegexPattern(pattern);
  ASSERT_TRUE(regex_pattern != nullptr);
  const std::unique_ptr<UniLib::RegexMatcher> matcher =
      regex_pattern->Matcher(message);
  ASSERT_TRUE(matcher != nullptr);
  int status = UniLib::RegexMatcher::kNoError;
  ASSERT_TRUE(matcher->Find(&status) &&
              status == UniLib::RegexMatcher::kNoError);

  EXPECT_TRUE(VerifyMatch(message.ToUTF8String(), matcher.get(), verifier));
}
#endif  // TC3_DISABLE_LUA

TEST_F(RegexMatchTest, RetrievesMatchGroupTest) {
  UnicodeText pattern =
      UTF8ToUnicodeText("never gonna (?:give (you) up|let (you) down)",
                        /*do_copy=*/true);
  const std::unique_ptr<UniLib::RegexPattern> regex_pattern =
      unilib_.CreateRegexPattern(pattern);
  ASSERT_TRUE(regex_pattern != nullptr);
  UnicodeText message =
      UTF8ToUnicodeText("never gonna give you up - never gonna let you down");
  const std::unique_ptr<UniLib::RegexMatcher> matcher =
      regex_pattern->Matcher(message);
  ASSERT_TRUE(matcher != nullptr);
  int status = UniLib::RegexMatcher::kNoError;

  ASSERT_TRUE(matcher->Find(&status) &&
              status == UniLib::RegexMatcher::kNoError);
  EXPECT_THAT(GetCapturingGroupText(matcher.get(), 0).value(),
              testing::Eq("never gonna give you up"));
  EXPECT_THAT(GetCapturingGroupText(matcher.get(), 1).value(),
              testing::Eq("you"));
  EXPECT_FALSE(GetCapturingGroupText(matcher.get(), 2).has_value());

  ASSERT_TRUE(matcher->Find(&status) &&
              status == UniLib::RegexMatcher::kNoError);
  EXPECT_THAT(GetCapturingGroupText(matcher.get(), 0).value(),
              testing::Eq("never gonna let you down"));
  EXPECT_FALSE(GetCapturingGroupText(matcher.get(), 1).has_value());
  EXPECT_THAT(GetCapturingGroupText(matcher.get(), 2).value(),
              testing::Eq("you"));
}
#endif

}  // namespace
}  // namespace libtextclassifier3
