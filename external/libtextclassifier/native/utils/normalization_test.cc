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

#include "utils/normalization.h"

#include <string>

#include "utils/base/integral_types.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::Eq;

class NormalizationTest : public testing::Test {
 protected:
  NormalizationTest() : INIT_UNILIB_FOR_TESTING(unilib_) {}

  std::string NormalizeTextCodepointWise(const std::string& text,
                                         const int32 codepointwise_ops) {
    return libtextclassifier3::NormalizeTextCodepointWise(
               unilib_, codepointwise_ops,
               UTF8ToUnicodeText(text, /*do_copy=*/false))
        .ToUTF8String();
  }

  UniLib unilib_;
};

TEST_F(NormalizationTest, ReturnsIdenticalStringWhenNoNormalization) {
  EXPECT_THAT(NormalizeTextCodepointWise(
                  "Never gonna let you down.",
                  NormalizationOptions_::CodepointwiseNormalizationOp_NONE),
              Eq("Never gonna let you down."));
}

#if !defined(TC3_UNILIB_DUMMY)
TEST_F(NormalizationTest, DropsWhitespace) {
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "Never gonna let you down.",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_WHITESPACE),
      Eq("Nevergonnaletyoudown."));
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "Never\tgonna\t\tlet\tyou\tdown.",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_WHITESPACE),
      Eq("Nevergonnaletyoudown."));
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "Never\u2003gonna\u2003let\u2003you\u2003down.",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_WHITESPACE),
      Eq("Nevergonnaletyoudown."));
}

TEST_F(NormalizationTest, DropsPunctuation) {
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "Never gonna let you down.",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_PUNCTUATION),
      Eq("Never gonna let you down"));
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "αʹ. Σημεῖόν ἐστιν, οὗ μέρος οὐθέν.",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_PUNCTUATION),
      Eq("αʹ Σημεῖόν ἐστιν οὗ μέρος οὐθέν"));
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "978—3—16—148410—0",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_PUNCTUATION),
      Eq("9783161484100"));
}

TEST_F(NormalizationTest, LowercasesUnicodeText) {
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "αʹ. Σημεῖόν ἐστιν, οὗ μέρος οὐθέν.",
          NormalizationOptions_::CodepointwiseNormalizationOp_LOWERCASE),
      Eq("αʹ. σημεῖόν ἐστιν, οὗ μέρος οὐθέν."));
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "αʹ. Σημεῖόν ἐστιν, οὗ μέρος οὐθέν.",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_WHITESPACE |
              NormalizationOptions_::CodepointwiseNormalizationOp_LOWERCASE),
      Eq("αʹ.σημεῖόνἐστιν,οὗμέροςοὐθέν."));
}

TEST_F(NormalizationTest, UppercasesUnicodeText) {
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "Κανένας άνθρωπος δεν ξέρει",
          NormalizationOptions_::CodepointwiseNormalizationOp_UPPERCASE),
      Eq("ΚΑΝΈΝΑΣ ΆΝΘΡΩΠΟΣ ΔΕΝ ΞΈΡΕΙ"));
  EXPECT_THAT(
      NormalizeTextCodepointWise(
          "Κανένας άνθρωπος δεν ξέρει",
          NormalizationOptions_::CodepointwiseNormalizationOp_DROP_WHITESPACE |
              NormalizationOptions_::CodepointwiseNormalizationOp_UPPERCASE),
      Eq("ΚΑΝΈΝΑΣΆΝΘΡΩΠΟΣΔΕΝΞΈΡΕΙ"));
}
#endif

}  // namespace
}  // namespace libtextclassifier3
