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

#ifndef LIBTEXTCLASSIFIER_UTILS_CONTAINER_SORTED_STRINGS_TABLE_H_
#define LIBTEXTCLASSIFIER_UTILS_CONTAINER_SORTED_STRINGS_TABLE_H_

#include <functional>
#include <vector>

#include "utils/base/integral_types.h"
#include "utils/container/string-set.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// A matcher to find string pieces matching prefixes of an input string.
// The list of reference strings are kept in sorted order in a zero separated
// string.
// binary search is used to find all prefix matches.
// num_pieces: Number of sentence pieces.
// offsets: Offsets into `pieces` where a string starts.
// pieces: String pieces, concatenated in sorted order and zero byte separated.
// use_linear_scan_threshold: Minimum size of binary search range before
//     switching to a linear sweep for prefix match testing.
class SortedStringsTable : public StringSet {
 public:
  SortedStringsTable(const int num_pieces, const uint32* offsets,
                     StringPiece pieces,
                     const int use_linear_scan_threshold = 10)
      : num_pieces_(num_pieces),
        offsets_(offsets),
        pieces_(pieces),
        use_linear_scan_threshold_(use_linear_scan_threshold) {}

  // Find matches that are prefixes of a string.
  bool FindAllPrefixMatches(StringPiece input,
                            std::vector<Match>* matches) const override;
  // Find the longest prefix match of a string.
  bool LongestPrefixMatch(StringPiece input,
                          Match* longest_match) const override;

 private:
  void GatherPrefixMatches(StringPiece input,
                           const std::function<void(Match)>& update_fn) const;

  const int num_pieces_;
  const uint32* offsets_;
  const StringPiece pieces_;
  const int use_linear_scan_threshold_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_CONTAINER_SORTED_STRINGS_TABLE_H_
