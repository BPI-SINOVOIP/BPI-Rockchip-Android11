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

#ifndef LIBTEXTCLASSIFIER_UTILS_CODEPOINT_RANGE_H_
#define LIBTEXTCLASSIFIER_UTILS_CODEPOINT_RANGE_H_

#include <vector>

#include "utils/base/integral_types.h"
#include "utils/codepoint-range_generated.h"

namespace libtextclassifier3 {

// Represents a codepoint range [start, end).
struct CodepointRangeStruct {
  int32 start;
  int32 end;

  CodepointRangeStruct(int32 arg_start, int32 arg_end)
      : start(arg_start), end(arg_end) {}
};

// Returns a sorted list of the codepoint (also converts the flatbuffer to
// struct).
void SortCodepointRanges(
    const std::vector<const CodepointRange*>& codepoint_ranges,
    std::vector<CodepointRangeStruct>* sorted_codepoint_ranges);

// Returns true if given codepoint is covered by the given sorted vector of
// codepoint ranges.
bool IsCodepointInRanges(
    int codepoint, const std::vector<CodepointRangeStruct>& codepoint_ranges);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_CODEPOINT_RANGE_H_
