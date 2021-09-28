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

#include "annotator/datetime/utils.h"

namespace libtextclassifier3 {

void FillInterpretations(const DatetimeParsedData& parse,
                         const DatetimeGranularity& granularity,
                         std::vector<DatetimeParsedData>* interpretations) {
  DatetimeParsedData modified_parse(parse);
  // If the relation field is not set, but relation_type field *is*, assume
  // the relation field is NEXT_OR_SAME. This is necessary to handle e.g.
  // "monday 3pm" (otherwise only "this monday 3pm" would work).
  if (parse.HasFieldType(DatetimeComponent::ComponentType::DAY_OF_WEEK)) {
    DatetimeComponent::RelativeQualifier relative_value;
    if (parse.GetRelativeValue(DatetimeComponent::ComponentType::DAY_OF_WEEK,
                               &relative_value)) {
      if (relative_value == DatetimeComponent::RelativeQualifier::UNSPECIFIED) {
        modified_parse.SetRelativeValue(
            DatetimeComponent::ComponentType::DAY_OF_WEEK,
            DatetimeComponent::RelativeQualifier::THIS);
      }
    }
  }

  // Multiple interpretations of ambiguous datetime expressions are generated
  // here.
  if (granularity > DatetimeGranularity::GRANULARITY_DAY &&
      modified_parse.HasFieldType(DatetimeComponent::ComponentType::HOUR) &&
      !modified_parse.HasRelativeValue(
          DatetimeComponent::ComponentType::HOUR) &&
      !modified_parse.HasFieldType(
          DatetimeComponent::ComponentType::MERIDIEM)) {
    int hour_value;
    modified_parse.GetFieldValue(DatetimeComponent::ComponentType::HOUR,
                                 &hour_value);
    if (hour_value <= 12) {
      modified_parse.SetAbsoluteValue(
          DatetimeComponent::ComponentType::MERIDIEM, 0);
      interpretations->push_back(modified_parse);
      modified_parse.SetAbsoluteValue(
          DatetimeComponent::ComponentType::MERIDIEM, 1);
      interpretations->push_back(modified_parse);
    } else {
      interpretations->push_back(modified_parse);
    }
  } else {
    // Otherwise just generate 1 variant.
    interpretations->push_back(modified_parse);
  }
}

}  // namespace libtextclassifier3
