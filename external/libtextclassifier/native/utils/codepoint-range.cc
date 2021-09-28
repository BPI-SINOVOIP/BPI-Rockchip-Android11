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

#include "utils/codepoint-range.h"

#include <algorithm>

namespace libtextclassifier3 {

// Returns a sorted list of the codepoint ranges.
void SortCodepointRanges(
    const std::vector<const CodepointRange*>& codepoint_ranges,
    std::vector<CodepointRangeStruct>* sorted_codepoint_ranges) {
  sorted_codepoint_ranges->clear();
  sorted_codepoint_ranges->reserve(codepoint_ranges.size());
  for (const CodepointRange* range : codepoint_ranges) {
    sorted_codepoint_ranges->push_back(
        CodepointRangeStruct(range->start(), range->end()));
  }

  std::sort(sorted_codepoint_ranges->begin(), sorted_codepoint_ranges->end(),
            [](const CodepointRangeStruct& a, const CodepointRangeStruct& b) {
              return a.start < b.start;
            });
}

// Returns true if given codepoint is covered by the given sorted vector of
// codepoint ranges.
bool IsCodepointInRanges(
    int codepoint, const std::vector<CodepointRangeStruct>& codepoint_ranges) {
  auto it = std::lower_bound(
      codepoint_ranges.begin(), codepoint_ranges.end(), codepoint,
      [](const CodepointRangeStruct& range, int codepoint) {
        // This function compares range with the
        // codepoint for the purpose of finding the first
        // greater or equal range. Because of the use of
        // std::lower_bound it needs to return true when
        // range < codepoint; the first time it will
        // return false the lower bound is found and
        // returned.
        //
        // It might seem weird that the condition is
        // range.end <= codepoint here but when codepoint
        // == range.end it means it's actually just
        // outside of the range, thus the range is less
        // than the codepoint.
        return range.end <= codepoint;
      });
  if (it != codepoint_ranges.end() && it->start <= codepoint &&
      it->end > codepoint) {
    return true;
  } else {
    return false;
  }
}

}  // namespace libtextclassifier3
