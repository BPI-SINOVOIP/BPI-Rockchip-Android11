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

#include <algorithm>

#include "utils/base/endian.h"
#include "utils/base/logging.h"

namespace libtextclassifier3 {

void SortedStringsTable::GatherPrefixMatches(
    StringPiece input, const std::function<void(Match)>& update_fn) const {
  int left = 0;
  int right = num_pieces_;
  int span_size = right - left;
  int match_length = 0;

  // Loop invariant:
  // at the ith iteration, all strings from `left` ... `right` match the input
  // on the first `match_length` characters.
  while (span_size > use_linear_scan_threshold_) {
    if (match_length >= input.length()) {
      return;
    }

    // We find the possible range of pieces in `left` ... `right` matching the
    // `match_length` + 1 character with two binary searches:
    //     `lower_bound` to find the start of the range of matching pieces.
    //     `upper_bound` to find the non-inclusive end of the range.
    left = (std::lower_bound(
                offsets_ + left, offsets_ + right,
                static_cast<unsigned char>(input[match_length]),
                [this, match_length](uint32 piece_offset, uint32 c) -> bool {
                  return static_cast<unsigned char>(
                             pieces_[piece_offset + match_length]) <
                         LittleEndian::ToHost32(c);
                }) -
            offsets_);
    right = (std::upper_bound(
                 offsets_ + left, offsets_ + right,
                 static_cast<unsigned char>(input[match_length]),
                 [this, match_length](uint32 c, uint32 piece_offset) -> bool {
                   return LittleEndian::ToHost32(c) <
                          static_cast<unsigned char>(
                              pieces_[piece_offset + match_length]);
                 }) -
             offsets_);
    span_size = right - left;
    if (span_size <= 0) {
      return;
    }
    ++match_length;

    // Due to the loop invariant and the fact that the strings are sorted, there
    // can only be one piece matching completely now, namely at left.
    if (pieces_[LittleEndian::ToHost32(offsets_[left]) + match_length] == 0) {
      update_fn(Match(/*id=*/left,
                      /*match_length=*/match_length));
      left++;
    }
  }

  // Use linear scan for small problem instances.
  // By the loop invariant characters 0...`match_length` of all pieces in
  // in `left`...`right` match the input on 0...`match_length`.
  for (int i = left; i < right; i++) {
    bool matches = true;
    int piece_match_length = match_length;
    for (int k = LittleEndian::ToHost32(offsets_[i]) + piece_match_length;
         pieces_[k] != 0; k++) {
      if (piece_match_length >= input.size() ||
          input[piece_match_length] != pieces_[k]) {
        matches = false;
        break;
      }
      piece_match_length++;
    }
    if (matches) {
      update_fn(Match(/*id=*/i,
                      /*match_length=*/piece_match_length));
    }
  }
}

bool SortedStringsTable::FindAllPrefixMatches(
    StringPiece input, std::vector<Match>* matches) const {
  GatherPrefixMatches(
      input, [matches](const Match match) { matches->push_back(match); });
  return true;
}

bool SortedStringsTable::LongestPrefixMatch(StringPiece input,
                                            Match* longest_match) const {
  *longest_match = Match();
  GatherPrefixMatches(
      input, [longest_match](const Match match) { *longest_match = match; });
  return true;
}

}  // namespace libtextclassifier3
