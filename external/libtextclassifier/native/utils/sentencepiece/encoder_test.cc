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

#include "utils/sentencepiece/encoder.h"

#include <memory>
#include <vector>

#include "utils/base/integral_types.h"
#include "utils/container/sorted-strings-table.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::ElementsAre;

TEST(EncoderTest, SimpleTokenization) {
  const char pieces_table[] = "hell\0hello\0o\0there\0";
  const uint32 offsets[] = {0, 5, 11, 13};
  float scores[] = {-0.5, -1.0, -10.0, -1.0};
  std::unique_ptr<StringSet> pieces(new SortedStringsTable(
      /*num_pieces=*/4, offsets, StringPiece(pieces_table, 18)));
  const Encoder encoder(pieces.get(),
                        /*num_pieces=*/4, scores);

  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellothere", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 3, 5, 1));
  }

  // Make probability of hello very low:
  // hello gets now tokenized as hell + o.
  scores[1] = -100.0;
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellothere", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 2, 4, 5, 1));
  }
}

TEST(EncoderTest, HandlesEdgeCases) {
  const char pieces_table[] = "hell\0hello\0o\0there\0";
  const uint32 offsets[] = {0, 5, 11, 13};
  float scores[] = {-0.5, -1.0, -10.0, -1.0};
  std::unique_ptr<StringSet> pieces(new SortedStringsTable(
      /*num_pieces=*/4, offsets, StringPiece(pieces_table, 18)));
  const Encoder encoder(pieces.get(),
                        /*num_pieces=*/4, scores);
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellhello", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 2, 3, 1));
  }
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellohell", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 3, 2, 1));
  }
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 1));
  }
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellathere", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 1));
  }
}

TEST(EncoderTest, HandlesOutOfDictionary) {
  const char pieces_table[] = "hell\0hello\0o\0there\0";
  const uint32 offsets[] = {0, 5, 11, 13};
  float scores[] = {-0.5, -1.0, -10.0, -1.0};
  std::unique_ptr<StringSet> pieces(new SortedStringsTable(
      /*num_pieces=*/4, offsets, StringPiece(pieces_table, 18)));
  const Encoder encoder(pieces.get(),
                        /*num_pieces=*/4, scores,
                        /*start_code=*/0, /*end_code=*/1,
                        /*encoding_offset=*/3, /*unknown_code=*/2,
                        /*unknown_score=*/-100.0);
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellhello", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 3, 4, 1));
  }
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellohell", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 4, 3, 1));
  }
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("", &encoded_text));
    EXPECT_THAT(encoded_text, ElementsAre(0, 1));
  }
  {
    std::vector<int> encoded_text;
    EXPECT_TRUE(encoder.Encode("hellathere", &encoded_text));
    EXPECT_THAT(encoded_text,
                ElementsAre(0, /*hell*/ 3, /*unknown*/ 2, /*there*/ 6, 1));
  }
}

}  // namespace
}  // namespace libtextclassifier3
