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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_OPTIONS_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_OPTIONS_H_

#include <string>
#include <vector>

#include "utils/base/integral_types.h"

namespace libtextclassifier3 {

// Options for date/datetime/date range annotations.
struct DateAnnotationOptions {
  // If enabled, extract special day offset like today, yesterday, etc.
  bool enable_special_day_offset;

  // If true, merge the adjacent day of week, time and date. e.g.
  // "20/2/2016 at 8pm" is extracted as a single instance instead of two
  // instance: "20/2/2016" and "8pm".
  bool merge_adjacent_components;

  // List the extra id of requested dates.
  std::vector<std::string> extra_requested_dates;

  // If true, try to include preposition to the extracted annotation. e.g.
  // "at 6pm". if it's false, only 6pm is included. offline-actions has special
  // requirements to include preposition.
  bool include_preposition;

  // The base timestamp (milliseconds) which used to convert relative time to
  // absolute time.
  // e.g.:
  //   base timestamp is 2016/4/25, then tomorrow will be converted to
  //   2016/4/26.
  //   base timestamp is 2016/4/25 10:30:20am, then 1 days, 2 hours, 10 minutes
  //   and 5 seconds ago will be converted to 2016/4/24 08:20:15am
  int64 base_timestamp_millis;

  // If enabled, extract range in date annotator.
  //   input: Monday, 5-6pm
  //     If the flag is true, The extracted annotation only contains 1 range
  //     instance which is from Monday 5pm to 6pm.
  //     If the flag is false, The extracted annotation contains two date
  //     instance: "Monday" and "6pm".
  bool enable_date_range;

  // Timezone in which the input text was written
  std::string reference_timezone;
  // Localization params.
  // The format of the locale lists should be "<lang_code-<county_code>"
  // comma-separated list of two-character language/country pairs.
  std::string locales;

  // If enabled, the annotation/rule_match priority score is used to set the and
  // priority score of the annotation.
  // In case of false the annotation priority score are set from
  // GrammarDatetimeModel's priority_score
  bool use_rule_priority_score;

  // If enabled, annotator will try to resolve the ambiguity by generating
  // possible alternative interpretations of the input text
  // e.g. '9:45' will be resolved to '9:45 AM' and '9:45 PM'.
  bool generate_alternative_interpretations_when_ambiguous;

  // List the ignored span in the date string e.g. 12 March @12PM, here '@'
  // can be ignored tokens.
  std::vector<std::string> ignored_spans;

  // Default Constructor
  DateAnnotationOptions()
      : enable_special_day_offset(true),
        merge_adjacent_components(true),
        include_preposition(false),
        base_timestamp_millis(0),
        enable_date_range(false),
        use_rule_priority_score(false),
        generate_alternative_interpretations_when_ambiguous(false) {}
};

}  // namespace libtextclassifier3
#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_OPTIONS_H_
