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

#include "annotator/grammar/dates/utils/date-match.h"

#include <algorithm>

#include "annotator/grammar/dates/utils/date-utils.h"
#include "annotator/types.h"
#include "utils/strings/append.h"

static const int kAM = 0;
static const int kPM = 1;

namespace libtextclassifier3 {
namespace dates {

namespace {
static int GetMeridiemValue(const TimespanCode& timespan_code) {
  switch (timespan_code) {
    case TimespanCode_AM:
    case TimespanCode_MIDNIGHT:
      // MIDNIGHT [3] -> AM
      return kAM;
    case TimespanCode_TONIGHT:
      // TONIGHT [11] -> PM
    case TimespanCode_NOON:
      // NOON [2] -> PM
    case TimespanCode_PM:
      return kPM;
    case TimespanCode_TIMESPAN_CODE_NONE:
    default:
      TC3_LOG(WARNING) << "Failed to extract time span code.";
  }
  return NO_VAL;
}

static int GetRelativeCount(const RelativeParameter* relative_parameter) {
  for (const int interpretation :
       *relative_parameter->day_of_week_interpretation()) {
    switch (interpretation) {
      case RelativeParameter_::Interpretation_NEAREST_LAST:
      case RelativeParameter_::Interpretation_PREVIOUS:
        return -1;
      case RelativeParameter_::Interpretation_SECOND_LAST:
        return -2;
      case RelativeParameter_::Interpretation_SECOND_NEXT:
        return 2;
      case RelativeParameter_::Interpretation_COMING:
      case RelativeParameter_::Interpretation_SOME:
      case RelativeParameter_::Interpretation_NEAREST:
      case RelativeParameter_::Interpretation_NEAREST_NEXT:
        return 1;
      case RelativeParameter_::Interpretation_CURRENT:
        return 0;
    }
  }
  return 0;
}
}  // namespace

using strings::JoinStrings;
using strings::SStringAppendF;

std::string DateMatch::DebugString() const {
  std::string res;
#if !defined(NDEBUG)
  if (begin >= 0 && end >= 0) {
    SStringAppendF(&res, 0, "[%u,%u)", begin, end);
  }

  if (HasDayOfWeek()) {
    SStringAppendF(&res, 0, "%u", day_of_week);
  }

  if (HasYear()) {
    int year_output = year;
    if (HasBcAd() && bc_ad == BCAD_BC) {
      year_output = -year;
    }
    SStringAppendF(&res, 0, "%u/", year_output);
  } else {
    SStringAppendF(&res, 0, "____/");
  }

  if (HasMonth()) {
    SStringAppendF(&res, 0, "%u/", month);
  } else {
    SStringAppendF(&res, 0, "__/");
  }

  if (HasDay()) {
    SStringAppendF(&res, 0, "%u ", day);
  } else {
    SStringAppendF(&res, 0, "__ ");
  }

  if (HasHour()) {
    SStringAppendF(&res, 0, "%u:", hour);
  } else {
    SStringAppendF(&res, 0, "__:");
  }

  if (HasMinute()) {
    SStringAppendF(&res, 0, "%u:", minute);
  } else {
    SStringAppendF(&res, 0, "__:");
  }

  if (HasSecond()) {
    if (HasFractionSecond()) {
      SStringAppendF(&res, 0, "%u.%lf ", second, fraction_second);
    } else {
      SStringAppendF(&res, 0, "%u ", second);
    }
  } else {
    SStringAppendF(&res, 0, "__ ");
  }

  if (HasTimeSpanCode() && TimespanCode_TIMESPAN_CODE_NONE < time_span_code &&
      time_span_code <= TimespanCode_MAX) {
    SStringAppendF(&res, 0, "TS=%u ", time_span_code);
  }

  if (HasTimeZoneCode() && time_zone_code != -1) {
    SStringAppendF(&res, 0, "TZ= %u ", time_zone_code);
  }

  if (HasTimeZoneOffset()) {
    SStringAppendF(&res, 0, "TZO=%u ", time_zone_offset);
  }

  if (HasRelativeDate()) {
    const RelativeMatch* rm = relative_match;
    SStringAppendF(&res, 0, (rm->is_future_date ? "future " : "past "));
    if (rm->day_of_week != NO_VAL) {
      SStringAppendF(&res, 0, "DOW:%d ", rm->day_of_week);
    }
    if (rm->year != NO_VAL) {
      SStringAppendF(&res, 0, "Y:%d ", rm->year);
    }
    if (rm->month != NO_VAL) {
      SStringAppendF(&res, 0, "M:%d ", rm->month);
    }
    if (rm->day != NO_VAL) {
      SStringAppendF(&res, 0, "D:%d ", rm->day);
    }
    if (rm->week != NO_VAL) {
      SStringAppendF(&res, 0, "W:%d ", rm->week);
    }
    if (rm->hour != NO_VAL) {
      SStringAppendF(&res, 0, "H:%d ", rm->hour);
    }
    if (rm->minute != NO_VAL) {
      SStringAppendF(&res, 0, "M:%d ", rm->minute);
    }
    if (rm->second != NO_VAL) {
      SStringAppendF(&res, 0, "S:%d ", rm->second);
    }
  }

  SStringAppendF(&res, 0, "prio=%d ", priority);
  SStringAppendF(&res, 0, "conf-score=%lf ", annotator_priority_score);

  if (IsHourAmbiguous()) {
    std::vector<int8> values;
    GetPossibleHourValues(&values);
    std::string str_values;

    for (unsigned int i = 0; i < values.size(); ++i) {
      SStringAppendF(&str_values, 0, "%u,", values[i]);
    }
    SStringAppendF(&res, 0, "amb=%s ", str_values.c_str());
  }

  std::vector<std::string> tags;
  if (is_inferred) {
    tags.push_back("inferred");
  }
  if (!tags.empty()) {
    SStringAppendF(&res, 0, "tag=%s ", JoinStrings(",", tags).c_str());
  }
#endif  // !defined(NDEBUG)
  return res;
}

void DateMatch::GetPossibleHourValues(std::vector<int8>* values) const {
  TC3_CHECK(values != nullptr);
  values->clear();
  if (HasHour()) {
    int8 possible_hour = hour;
    values->push_back(possible_hour);
    for (int count = 1; count < ambiguous_hour_count; ++count) {
      possible_hour += ambiguous_hour_interval;
      if (possible_hour >= 24) {
        possible_hour -= 24;
      }
      values->push_back(possible_hour);
    }
  }
}

DatetimeComponent::RelativeQualifier DateMatch::GetRelativeQualifier() const {
  if (HasRelativeDate()) {
    if (relative_match->existing & RelativeMatch::HAS_IS_FUTURE) {
      if (!relative_match->is_future_date) {
        return DatetimeComponent::RelativeQualifier::PAST;
      }
    }
    return DatetimeComponent::RelativeQualifier::FUTURE;
  }
  return DatetimeComponent::RelativeQualifier::UNSPECIFIED;
}

// Embed RelativeQualifier information of DatetimeComponent as a sign of
// relative counter field of datetime component i.e. relative counter is
// negative when relative qualifier RelativeQualifier::PAST.
int GetAdjustedRelativeCounter(
    const DatetimeComponent::RelativeQualifier& relative_qualifier,
    const int relative_counter) {
  if (DatetimeComponent::RelativeQualifier::PAST == relative_qualifier) {
    return -relative_counter;
  }
  return relative_counter;
}

Optional<DatetimeComponent> CreateDatetimeComponent(
    const DatetimeComponent::ComponentType& component_type,
    const DatetimeComponent::RelativeQualifier& relative_qualifier,
    const int absolute_value, const int relative_value) {
  if (absolute_value == NO_VAL && relative_value == NO_VAL) {
    return Optional<DatetimeComponent>();
  }
  return Optional<DatetimeComponent>(DatetimeComponent(
      component_type,
      (relative_value != NO_VAL)
          ? relative_qualifier
          : DatetimeComponent::RelativeQualifier::UNSPECIFIED,
      (absolute_value != NO_VAL) ? absolute_value : 0,
      (relative_value != NO_VAL)
          ? GetAdjustedRelativeCounter(relative_qualifier, relative_value)
          : 0));
}

Optional<DatetimeComponent> CreateDayOfWeekComponent(
    const RelativeMatch* relative_match,
    const DatetimeComponent::RelativeQualifier& relative_qualifier,
    const DayOfWeek& absolute_day_of_week) {
  DatetimeComponent::RelativeQualifier updated_relative_qualifier =
      relative_qualifier;
  int absolute_value = absolute_day_of_week;
  int relative_value = NO_VAL;
  if (relative_match) {
    relative_value = relative_match->day_of_week;
    if (relative_match->existing & RelativeMatch::HAS_DAY_OF_WEEK) {
      if (relative_match->IsStandaloneRelativeDayOfWeek() &&
          absolute_day_of_week == DayOfWeek_DOW_NONE) {
        absolute_value = relative_match->day_of_week;
      }
      // Check if the relative date has day of week with week period.
      if (relative_match->existing & RelativeMatch::HAS_WEEK) {
        relative_value = 1;
      } else {
        const NonterminalValue* nonterminal =
            relative_match->day_of_week_nonterminal;
        TC3_CHECK(nonterminal != nullptr);
        TC3_CHECK(nonterminal->relative_parameter());
        const RelativeParameter* rp = nonterminal->relative_parameter();
        if (rp->day_of_week_interpretation()) {
          relative_value = GetRelativeCount(rp);
          if (relative_value < 0) {
            relative_value = abs(relative_value);
            updated_relative_qualifier =
                DatetimeComponent::RelativeQualifier::PAST;
          } else if (relative_value > 0) {
            updated_relative_qualifier =
                DatetimeComponent::RelativeQualifier::FUTURE;
          }
        }
      }
    }
  }
  return CreateDatetimeComponent(DatetimeComponent::ComponentType::DAY_OF_WEEK,
                                 updated_relative_qualifier, absolute_value,
                                 relative_value);
}

// Resolve the  year’s ambiguity.
// If the year in the date has 4 digits i.e. DD/MM/YYYY then there is no
// ambiguity, the year value is YYYY but certain format i.e. MM/DD/YY is
// ambiguous e.g. in {April/23/15} year value can be 15 or 1915 or 2015.
// Following heuristic is used to resolve the ambiguity.
// - For YYYY there is nothing to resolve.
// - For all YY years
//    - Value less than 50 will be resolved to 20YY
//    - Value greater or equal 50 will be resolved to 19YY
static int InterpretYear(int parsed_year) {
  if (parsed_year == NO_VAL) {
    return parsed_year;
  }
  if (parsed_year < 100) {
    if (parsed_year < 50) {
      return parsed_year + 2000;
    }
    return parsed_year + 1900;
  }
  return parsed_year;
}

Optional<DatetimeComponent> DateMatch::GetDatetimeComponent(
    const DatetimeComponent::ComponentType& component_type) const {
  switch (component_type) {
    case DatetimeComponent::ComponentType::YEAR:
      return CreateDatetimeComponent(
          component_type, GetRelativeQualifier(), InterpretYear(year),
          (relative_match != nullptr) ? relative_match->year : NO_VAL);
    case DatetimeComponent::ComponentType::MONTH:
      return CreateDatetimeComponent(
          component_type, GetRelativeQualifier(), month,
          (relative_match != nullptr) ? relative_match->month : NO_VAL);
    case DatetimeComponent::ComponentType::DAY_OF_MONTH:
      return CreateDatetimeComponent(
          component_type, GetRelativeQualifier(), day,
          (relative_match != nullptr) ? relative_match->day : NO_VAL);
    case DatetimeComponent::ComponentType::HOUR:
      return CreateDatetimeComponent(
          component_type, GetRelativeQualifier(), hour,
          (relative_match != nullptr) ? relative_match->hour : NO_VAL);
    case DatetimeComponent::ComponentType::MINUTE:
      return CreateDatetimeComponent(
          component_type, GetRelativeQualifier(), minute,
          (relative_match != nullptr) ? relative_match->minute : NO_VAL);
    case DatetimeComponent::ComponentType::SECOND:
      return CreateDatetimeComponent(
          component_type, GetRelativeQualifier(), second,
          (relative_match != nullptr) ? relative_match->second : NO_VAL);
    case DatetimeComponent::ComponentType::DAY_OF_WEEK:
      return CreateDayOfWeekComponent(relative_match, GetRelativeQualifier(),
                                      day_of_week);
    case DatetimeComponent::ComponentType::MERIDIEM:
      return CreateDatetimeComponent(component_type, GetRelativeQualifier(),
                                     GetMeridiemValue(time_span_code), NO_VAL);
    case DatetimeComponent::ComponentType::ZONE_OFFSET:
      if (HasTimeZoneOffset()) {
        return Optional<DatetimeComponent>(DatetimeComponent(
            component_type, DatetimeComponent::RelativeQualifier::UNSPECIFIED,
            time_zone_offset, /*arg_relative_count=*/0));
      }
      return Optional<DatetimeComponent>();
    case DatetimeComponent::ComponentType::WEEK:
      return CreateDatetimeComponent(
          component_type, GetRelativeQualifier(), NO_VAL,
          HasRelativeDate() ? relative_match->week : NO_VAL);
    default:
      return Optional<DatetimeComponent>();
  }
}

bool DateMatch::IsValid() const {
  if (!HasYear() && HasBcAd()) {
    return false;
  }
  if (!HasMonth() && HasYear() && (HasDay() || HasDayOfWeek())) {
    return false;
  }
  if (!HasDay() && HasDayOfWeek() && (HasYear() || HasMonth())) {
    return false;
  }
  if (!HasDay() && !HasDayOfWeek() && HasHour() && (HasYear() || HasMonth())) {
    return false;
  }
  if (!HasHour() && (HasMinute() || HasSecond() || HasFractionSecond())) {
    return false;
  }
  if (!HasMinute() && (HasSecond() || HasFractionSecond())) {
    return false;
  }
  if (!HasSecond() && HasFractionSecond()) {
    return false;
  }
  // Check whether day exists in a month, to exclude cases like "April 31".
  if (HasDay() && HasMonth() && day > GetLastDayOfMonth(year, month)) {
    return false;
  }
  return (HasDateFields() || HasTimeFields() || HasRelativeDate());
}

void DateMatch::FillDatetimeComponents(
    std::vector<DatetimeComponent>* datetime_component) const {
  static const std::vector<DatetimeComponent::ComponentType>*
      kDatetimeComponents = new std::vector<DatetimeComponent::ComponentType>{
          DatetimeComponent::ComponentType::ZONE_OFFSET,
          DatetimeComponent::ComponentType::MERIDIEM,
          DatetimeComponent::ComponentType::SECOND,
          DatetimeComponent::ComponentType::MINUTE,
          DatetimeComponent::ComponentType::HOUR,
          DatetimeComponent::ComponentType::DAY_OF_MONTH,
          DatetimeComponent::ComponentType::DAY_OF_WEEK,
          DatetimeComponent::ComponentType::WEEK,
          DatetimeComponent::ComponentType::MONTH,
          DatetimeComponent::ComponentType::YEAR};

  for (const DatetimeComponent::ComponentType& component_type :
       *kDatetimeComponents) {
    Optional<DatetimeComponent> date_time =
        GetDatetimeComponent(component_type);
    if (date_time.has_value()) {
      datetime_component->emplace_back(date_time.value());
    }
  }
}

std::string DateRangeMatch::DebugString() const {
  std::string res;
  // The method is only called for debugging purposes.
#if !defined(NDEBUG)
  if (begin >= 0 && end >= 0) {
    SStringAppendF(&res, 0, "[%u,%u)\n", begin, end);
  }
  SStringAppendF(&res, 0, "from: %s \n", from.DebugString().c_str());
  SStringAppendF(&res, 0, "to: %s\n", to.DebugString().c_str());
#endif  // !defined(NDEBUG)
  return res;
}

}  // namespace dates
}  // namespace libtextclassifier3
