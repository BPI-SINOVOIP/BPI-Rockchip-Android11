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

#include "annotator/datetime/extractor.h"

#include "utils/base/logging.h"

namespace libtextclassifier3 {

bool DatetimeExtractor::Extract(DatetimeParsedData* result,
                                CodepointSpan* result_span) const {
  *result_span = {kInvalidIndex, kInvalidIndex};

  if (rule_.regex->groups() == nullptr) {
    return false;
  }

  // In the current implementation of extractor, the assumption is that there
  // can only be one relative field.
  DatetimeComponent::ComponentType component_type;
  DatetimeComponent::RelativeQualifier relative_qualifier =
      DatetimeComponent::RelativeQualifier::UNSPECIFIED;
  int relative_count = 0;

  for (int group_id = 0; group_id < rule_.regex->groups()->size(); group_id++) {
    UnicodeText group_text;
    const int group_type = rule_.regex->groups()->Get(group_id);
    if (group_type == DatetimeGroupType_GROUP_UNUSED) {
      continue;
    }
    if (!GroupTextFromMatch(group_id, &group_text)) {
      TC3_LOG(ERROR) << "Couldn't retrieve group.";
      return false;
    }
    // The pattern can have a group defined in a part that was not matched,
    // e.g. an optional part. In this case we'll get an empty content here.
    if (group_text.empty()) {
      continue;
    }

    switch (group_type) {
      case DatetimeGroupType_GROUP_YEAR: {
        int year;
        if (!ParseYear(group_text, &(year))) {
          TC3_LOG(ERROR) << "Couldn't extract YEAR.";
          return false;
        }
        result->SetAbsoluteValue(DatetimeComponent::ComponentType::YEAR, year);
        break;
      }
      case DatetimeGroupType_GROUP_MONTH: {
        int month;
        if (!ParseMonth(group_text, &(month))) {
          TC3_LOG(ERROR) << "Couldn't extract MONTH.";
          return false;
        }
        result->SetAbsoluteValue(DatetimeComponent::ComponentType::MONTH,
                                 month);
        break;
      }
      case DatetimeGroupType_GROUP_DAY: {
        int day_of_month;
        if (!ParseDigits(group_text, &(day_of_month))) {
          TC3_LOG(ERROR) << "Couldn't extract DAY.";
          return false;
        }
        result->SetAbsoluteValue(DatetimeComponent::ComponentType::DAY_OF_MONTH,
                                 day_of_month);
        break;
      }
      case DatetimeGroupType_GROUP_HOUR: {
        int hour;
        if (!ParseDigits(group_text, &(hour))) {
          TC3_LOG(ERROR) << "Couldn't extract HOUR.";
          return false;
        }
        result->SetAbsoluteValue(DatetimeComponent::ComponentType::HOUR, hour);
        break;
      }
      case DatetimeGroupType_GROUP_MINUTE: {
        int minute;
        if (!ParseDigits(group_text, &(minute)) &&
            !ParseWrittenNumber(group_text, &(minute))) {
          TC3_LOG(ERROR) << "Couldn't extract MINUTE.";
          return false;
        }
        result->SetAbsoluteValue(DatetimeComponent::ComponentType::MINUTE,
                                 minute);
        break;
      }
      case DatetimeGroupType_GROUP_SECOND: {
        int second;
        if (!ParseDigits(group_text, &(second))) {
          TC3_LOG(ERROR) << "Couldn't extract SECOND.";
          return false;
        }
        result->SetAbsoluteValue(DatetimeComponent::ComponentType::SECOND,
                                 second);
        break;
      }
      case DatetimeGroupType_GROUP_AMPM: {
        int meridiem;
        if (!ParseMeridiem(group_text, &(meridiem))) {
          TC3_LOG(ERROR) << "Couldn't extract AMPM.";
          return false;
        }
        result->SetAbsoluteValue(DatetimeComponent::ComponentType::MERIDIEM,
                                 meridiem);
        break;
      }
      case DatetimeGroupType_GROUP_RELATIONDISTANCE: {
        relative_count = 0;
        if (!ParseRelationDistance(group_text, &(relative_count))) {
          TC3_LOG(ERROR) << "Couldn't extract RELATION_DISTANCE_FIELD.";
          return false;
        }
        break;
      }
      case DatetimeGroupType_GROUP_RELATION: {
        if (!ParseRelativeValue(group_text, &relative_qualifier)) {
          TC3_LOG(ERROR) << "Couldn't extract RELATION_FIELD.";
          return false;
        }
        ParseRelationAndConvertToRelativeCount(group_text, &relative_count);
        if (relative_qualifier ==
                DatetimeComponent::RelativeQualifier::TOMORROW ||
            relative_qualifier == DatetimeComponent::RelativeQualifier::NOW ||
            relative_qualifier ==
                DatetimeComponent::RelativeQualifier::YESTERDAY) {
          if (!ParseFieldType(group_text, &component_type)) {
            TC3_LOG(ERROR) << "Couldn't extract RELATION_TYPE_FIELD.";
            return false;
          }
        }
        break;
      }
      case DatetimeGroupType_GROUP_RELATIONTYPE: {
        if (!ParseFieldType(group_text, &component_type)) {
          TC3_LOG(ERROR) << "Couldn't extract RELATION_TYPE_FIELD.";
          return false;
        }
        if (component_type == DatetimeComponent::ComponentType::DAY_OF_WEEK) {
          int day_of_week;
          if (!ParseDayOfWeek(group_text, &day_of_week)) {
            TC3_LOG(ERROR) << "Couldn't extract RELATION_TYPE_FIELD.";
            return false;
          }
          result->SetAbsoluteValue(component_type, day_of_week);
        }
        break;
      }
      case DatetimeGroupType_GROUP_DUMMY1:
      case DatetimeGroupType_GROUP_DUMMY2:
        break;
      default:
        TC3_LOG(INFO) << "Unknown group type.";
        continue;
    }
    if (!UpdateMatchSpan(group_id, result_span)) {
      TC3_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (relative_qualifier != DatetimeComponent::RelativeQualifier::UNSPECIFIED) {
    result->SetRelativeValue(component_type, relative_qualifier);
    result->SetRelativeCount(component_type, relative_count);
  }

  if (result_span->first == kInvalidIndex ||
      result_span->second == kInvalidIndex) {
    *result_span = {kInvalidIndex, kInvalidIndex};
  }

  return true;
}

bool DatetimeExtractor::RuleIdForType(DatetimeExtractorType type,
                                      int* rule_id) const {
  auto type_it = type_and_locale_to_rule_.find(type);
  if (type_it == type_and_locale_to_rule_.end()) {
    return false;
  }

  auto locale_it = type_it->second.find(locale_id_);
  if (locale_it == type_it->second.end()) {
    return false;
  }
  *rule_id = locale_it->second;
  return true;
}

bool DatetimeExtractor::ExtractType(const UnicodeText& input,
                                    DatetimeExtractorType extractor_type,
                                    UnicodeText* match_result) const {
  int rule_id;
  if (!RuleIdForType(extractor_type, &rule_id)) {
    return false;
  }

  std::unique_ptr<UniLib::RegexMatcher> matcher =
      rules_[rule_id]->Matcher(input);
  if (!matcher) {
    return false;
  }

  int status;
  if (!matcher->Find(&status)) {
    return false;
  }

  if (match_result != nullptr) {
    *match_result = matcher->Group(&status);
    if (status != UniLib::RegexMatcher::kNoError) {
      return false;
    }
  }
  return true;
}

bool DatetimeExtractor::GroupTextFromMatch(int group_id,
                                           UnicodeText* result) const {
  int status;
  *result = matcher_.Group(group_id, &status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }
  return true;
}

bool DatetimeExtractor::UpdateMatchSpan(int group_id,
                                        CodepointSpan* span) const {
  int status;
  const int match_start = matcher_.Start(group_id, &status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }
  const int match_end = matcher_.End(group_id, &status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }
  if (span->first == kInvalidIndex || span->first > match_start) {
    span->first = match_start;
  }
  if (span->second == kInvalidIndex || span->second < match_end) {
    span->second = match_end;
  }

  return true;
}

template <typename T>
bool DatetimeExtractor::MapInput(
    const UnicodeText& input,
    const std::vector<std::pair<DatetimeExtractorType, T>>& mapping,
    T* result) const {
  for (const auto& type_value_pair : mapping) {
    if (ExtractType(input, type_value_pair.first)) {
      *result = type_value_pair.second;
      return true;
    }
  }
  return false;
}

bool DatetimeExtractor::ParseWrittenNumber(const UnicodeText& input,
                                           int* parsed_number) const {
  std::vector<std::pair<int, int>> found_numbers;
  for (const auto& type_value_pair :
       std::vector<std::pair<DatetimeExtractorType, int>>{
           {DatetimeExtractorType_ZERO, 0},
           {DatetimeExtractorType_ONE, 1},
           {DatetimeExtractorType_TWO, 2},
           {DatetimeExtractorType_THREE, 3},
           {DatetimeExtractorType_FOUR, 4},
           {DatetimeExtractorType_FIVE, 5},
           {DatetimeExtractorType_SIX, 6},
           {DatetimeExtractorType_SEVEN, 7},
           {DatetimeExtractorType_EIGHT, 8},
           {DatetimeExtractorType_NINE, 9},
           {DatetimeExtractorType_TEN, 10},
           {DatetimeExtractorType_ELEVEN, 11},
           {DatetimeExtractorType_TWELVE, 12},
           {DatetimeExtractorType_THIRTEEN, 13},
           {DatetimeExtractorType_FOURTEEN, 14},
           {DatetimeExtractorType_FIFTEEN, 15},
           {DatetimeExtractorType_SIXTEEN, 16},
           {DatetimeExtractorType_SEVENTEEN, 17},
           {DatetimeExtractorType_EIGHTEEN, 18},
           {DatetimeExtractorType_NINETEEN, 19},
           {DatetimeExtractorType_TWENTY, 20},
           {DatetimeExtractorType_THIRTY, 30},
           {DatetimeExtractorType_FORTY, 40},
           {DatetimeExtractorType_FIFTY, 50},
           {DatetimeExtractorType_SIXTY, 60},
           {DatetimeExtractorType_SEVENTY, 70},
           {DatetimeExtractorType_EIGHTY, 80},
           {DatetimeExtractorType_NINETY, 90},
           {DatetimeExtractorType_HUNDRED, 100},
           {DatetimeExtractorType_THOUSAND, 1000},
       }) {
    int rule_id;
    if (!RuleIdForType(type_value_pair.first, &rule_id)) {
      return false;
    }

    std::unique_ptr<UniLib::RegexMatcher> matcher =
        rules_[rule_id]->Matcher(input);
    if (!matcher) {
      return false;
    }
    int status;
    while (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
      int span_start = matcher->Start(&status);
      if (status != UniLib::RegexMatcher::kNoError) {
        return false;
      }
      found_numbers.push_back({span_start, type_value_pair.second});
    }
  }

  std::sort(found_numbers.begin(), found_numbers.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
              return a.first < b.first;
            });

  int sum = 0;
  int running_value = -1;
  // Simple math to make sure we handle written numerical modifiers correctly
  // so that :="fifty one  thousand and one" maps to 51001 and not 50 1 1000 1.
  for (const std::pair<int, int>& position_number_pair : found_numbers) {
    if (running_value >= 0) {
      if (running_value > position_number_pair.second) {
        sum += running_value;
        running_value = position_number_pair.second;
      } else {
        running_value *= position_number_pair.second;
      }
    } else {
      running_value = position_number_pair.second;
    }
  }
  sum += running_value;
  *parsed_number = sum;
  return true;
}

bool DatetimeExtractor::ParseDigits(const UnicodeText& input,
                                    int* parsed_digits) const {
  UnicodeText digit;
  if (!ExtractType(input, DatetimeExtractorType_DIGITS, &digit)) {
    return false;
  }

  if (!unilib_.ParseInt32(digit, parsed_digits)) {
    return false;
  }
  return true;
}

bool DatetimeExtractor::ParseYear(const UnicodeText& input,
                                  int* parsed_year) const {
  if (!ParseDigits(input, parsed_year)) {
    return false;
  }

  // Logic to decide if XX will be 20XX or 19XX
  if (*parsed_year < 100) {
    if (*parsed_year < 50) {
      *parsed_year += 2000;
    } else {
      *parsed_year += 1900;
    }
  }

  return true;
}

bool DatetimeExtractor::ParseMonth(const UnicodeText& input,
                                   int* parsed_month) const {
  if (ParseDigits(input, parsed_month)) {
    return true;
  }

  if (MapInput(input,
               {
                   {DatetimeExtractorType_JANUARY, 1},
                   {DatetimeExtractorType_FEBRUARY, 2},
                   {DatetimeExtractorType_MARCH, 3},
                   {DatetimeExtractorType_APRIL, 4},
                   {DatetimeExtractorType_MAY, 5},
                   {DatetimeExtractorType_JUNE, 6},
                   {DatetimeExtractorType_JULY, 7},
                   {DatetimeExtractorType_AUGUST, 8},
                   {DatetimeExtractorType_SEPTEMBER, 9},
                   {DatetimeExtractorType_OCTOBER, 10},
                   {DatetimeExtractorType_NOVEMBER, 11},
                   {DatetimeExtractorType_DECEMBER, 12},
               },
               parsed_month)) {
    return true;
  }

  return false;
}

bool DatetimeExtractor::ParseMeridiem(const UnicodeText& input,
                                      int* parsed_meridiem) const {
  return MapInput(input,
                  {
                      {DatetimeExtractorType_AM, 0 /* AM */},
                      {DatetimeExtractorType_PM, 1 /* PM */},
                  },
                  parsed_meridiem);
}

bool DatetimeExtractor::ParseRelationDistance(const UnicodeText& input,
                                              int* parsed_distance) const {
  if (ParseDigits(input, parsed_distance)) {
    return true;
  }
  if (ParseWrittenNumber(input, parsed_distance)) {
    return true;
  }
  return false;
}

bool DatetimeExtractor::ParseRelativeValue(
    const UnicodeText& input,
    DatetimeComponent::RelativeQualifier* parsed_relative_value) const {
  return MapInput(input,
                  {
                      {DatetimeExtractorType_NOW,
                       DatetimeComponent::RelativeQualifier::NOW},
                      {DatetimeExtractorType_YESTERDAY,
                       DatetimeComponent::RelativeQualifier::YESTERDAY},
                      {DatetimeExtractorType_TOMORROW,
                       DatetimeComponent::RelativeQualifier::TOMORROW},
                      {DatetimeExtractorType_NEXT,
                       DatetimeComponent::RelativeQualifier::NEXT},
                      {DatetimeExtractorType_NEXT_OR_SAME,
                       DatetimeComponent::RelativeQualifier::THIS},
                      {DatetimeExtractorType_LAST,
                       DatetimeComponent::RelativeQualifier::LAST},
                      {DatetimeExtractorType_PAST,
                       DatetimeComponent::RelativeQualifier::PAST},
                      {DatetimeExtractorType_FUTURE,
                       DatetimeComponent::RelativeQualifier::FUTURE},
                  },
                  parsed_relative_value);
}

bool DatetimeExtractor::ParseRelationAndConvertToRelativeCount(
    const UnicodeText& input, int* relative_count) const {
  return MapInput(input,
                  {
                      {DatetimeExtractorType_NOW, 0},
                      {DatetimeExtractorType_YESTERDAY, -1},
                      {DatetimeExtractorType_TOMORROW, 1},
                      {DatetimeExtractorType_NEXT, 1},
                      {DatetimeExtractorType_NEXT_OR_SAME, 1},
                      {DatetimeExtractorType_LAST, -1},
                      {DatetimeExtractorType_PAST, -1},
                  },
                  relative_count);
}

bool DatetimeExtractor::ParseDayOfWeek(const UnicodeText& input,
                                       int* parsed_day_of_week) const {
  return MapInput(input,
                  {
                      {DatetimeExtractorType_SUNDAY, kSunday},
                      {DatetimeExtractorType_MONDAY, kMonday},
                      {DatetimeExtractorType_TUESDAY, kTuesday},
                      {DatetimeExtractorType_WEDNESDAY, kWednesday},
                      {DatetimeExtractorType_THURSDAY, kThursday},
                      {DatetimeExtractorType_FRIDAY, kFriday},
                      {DatetimeExtractorType_SATURDAY, kSaturday},
                  },
                  parsed_day_of_week);
}

bool DatetimeExtractor::ParseFieldType(
    const UnicodeText& input,
    DatetimeComponent::ComponentType* parsed_field_type) const {
  return MapInput(
      input,
      {
          {DatetimeExtractorType_MONDAY,
           DatetimeComponent::ComponentType::DAY_OF_WEEK},
          {DatetimeExtractorType_TUESDAY,
           DatetimeComponent::ComponentType::DAY_OF_WEEK},
          {DatetimeExtractorType_WEDNESDAY,
           DatetimeComponent::ComponentType::DAY_OF_WEEK},
          {DatetimeExtractorType_THURSDAY,
           DatetimeComponent::ComponentType::DAY_OF_WEEK},
          {DatetimeExtractorType_FRIDAY,
           DatetimeComponent::ComponentType::DAY_OF_WEEK},
          {DatetimeExtractorType_SATURDAY,
           DatetimeComponent::ComponentType::DAY_OF_WEEK},
          {DatetimeExtractorType_SUNDAY,
           DatetimeComponent::ComponentType::DAY_OF_WEEK},
          {DatetimeExtractorType_SECONDS,
           DatetimeComponent::ComponentType::SECOND},
          {DatetimeExtractorType_MINUTES,
           DatetimeComponent::ComponentType::MINUTE},
          {DatetimeExtractorType_NOW,
           DatetimeComponent::ComponentType::DAY_OF_MONTH},
          {DatetimeExtractorType_HOURS, DatetimeComponent::ComponentType::HOUR},
          {DatetimeExtractorType_DAY,
           DatetimeComponent::ComponentType::DAY_OF_MONTH},
          {DatetimeExtractorType_TOMORROW,
           DatetimeComponent::ComponentType::DAY_OF_MONTH},
          {DatetimeExtractorType_YESTERDAY,
           DatetimeComponent::ComponentType::DAY_OF_MONTH},
          {DatetimeExtractorType_WEEK, DatetimeComponent::ComponentType::WEEK},
          {DatetimeExtractorType_MONTH,
           DatetimeComponent::ComponentType::MONTH},
          {DatetimeExtractorType_YEAR, DatetimeComponent::ComponentType::YEAR},
      },
      parsed_field_type);
}

}  // namespace libtextclassifier3
