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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_DATE_UTILS_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_DATE_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <ctime>
#include <vector>

#include "annotator/grammar/dates/annotations/annotation.h"
#include "annotator/grammar/dates/utils/date-match.h"
#include "utils/base/casts.h"

namespace libtextclassifier3 {
namespace dates {

bool IsLeapYear(int year);

int8 GetLastDayOfMonth(int year, int month);

// Normalizes hour value of the specified date using the specified time-span
// specification. Returns true if the original hour value (can be no-value)
// is compatible with the time-span and gets normalized successfully, or
// false otherwise.
bool NormalizeHourByTimeSpan(const TimeSpanSpec* ts_spec, DateMatch* date);

// Returns true iff "a" is considered as a refinement of "b". For example,
// besides fully compatible fields, having more fields or higher priority.
bool IsRefinement(const DateMatch& a, const DateMatch& b);
bool IsRefinement(const DateRangeMatch& a, const DateRangeMatch& b);

// Returns true iff "a" occurs strictly before "b"
bool IsPrecedent(const DateMatch& a, const DateMatch& b);

// Fill DatetimeParseResult based on DateMatch object which is created from
// matched rule. The matched string is extracted from tokenizer which provides
// an interface to access the clean text based on the matched range.
void FillDateInstance(const DateMatch& date, DatetimeParseResult* instance);

// Fill DatetimeParseResultSpan based on DateMatch object which is created from
// matched rule. The matched string is extracted from tokenizer which provides
// an interface to access the clean text based on the matched range.
void FillDateInstance(const DateMatch& date, DatetimeParseResultSpan* instance);

// Fill DatetimeParseResultSpan based on DateRangeMatch object which i screated
// from matched rule.
void FillDateRangeInstance(const DateRangeMatch& range,
                           DatetimeParseResultSpan* instance);

// Merge the fields in DateMatch prev to next if there is no overlapped field.
// If update_span is true, the span of next is also updated.
// e.g.: prev is 11am, next is: May 1, then the merged next is May 1, 11am
void MergeDateMatch(const DateMatch& prev, DateMatch* next, bool update_span);

// If DateMatches have no overlapped field, then they could be merged as the
// following rules:
//   -- If both don't have relative match and one DateMatch has day but another
//      DateMatch has hour.
//   -- If one have relative match then follow the rules in code.
// It's impossible to get DateMatch which only has DOW and not in relative
// match according to current rules.
bool IsDateMatchMergeable(const DateMatch& prev, const DateMatch& next);
}  // namespace dates
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_DATE_UTILS_H_
