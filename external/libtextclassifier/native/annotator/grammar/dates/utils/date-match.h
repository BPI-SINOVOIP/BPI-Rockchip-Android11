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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_DATE_MATCH_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_DATE_MATCH_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "annotator/grammar/dates/dates_generated.h"
#include "annotator/grammar/dates/timezone-code_generated.h"
#include "utils/grammar/match.h"

namespace libtextclassifier3 {
namespace dates {

static constexpr int NO_VAL = -1;

// POD match data structure.
struct MatchBase : public grammar::Match {
  void Reset() { type = MatchType::MatchType_UNKNOWN; }
};

struct ExtractionMatch : public MatchBase {
  const ExtractionRuleParameter* extraction_rule;

  void Reset() {
    MatchBase::Reset();
    type = MatchType::MatchType_DATETIME_RULE;
    extraction_rule = nullptr;
  }
};

struct TermValueMatch : public MatchBase {
  const TermValue* term_value;

  void Reset() {
    MatchBase::Reset();
    type = MatchType::MatchType_TERM_VALUE;
    term_value = nullptr;
  }
};

struct NonterminalMatch : public MatchBase {
  const NonterminalValue* nonterminal;

  void Reset() {
    MatchBase::Reset();
    type = MatchType::MatchType_NONTERMINAL;
    nonterminal = nullptr;
  }
};

struct IntegerMatch : public NonterminalMatch {
  int value;
  int8 count_of_digits;   // When expression is in digits format.
  bool is_zero_prefixed;  // When expression is in digits format.

  void Reset() {
    NonterminalMatch::Reset();
    value = NO_VAL;
    count_of_digits = 0;
    is_zero_prefixed = false;
  }
};

struct DigitsMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_DIGITS;
  }

  static bool IsValid(int x) { return true; }
};

struct YearMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_YEAR;
  }

  static bool IsValid(int x) { return x >= 1; }
};

struct MonthMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_MONTH;
  }

  static bool IsValid(int x) { return (x >= 1 && x <= 12); }
};

struct DayMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_DAY;
  }

  static bool IsValid(int x) { return (x >= 1 && x <= 31); }
};

struct HourMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_HOUR;
  }

  static bool IsValid(int x) { return (x >= 0 && x <= 24); }
};

struct MinuteMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_MINUTE;
  }

  static bool IsValid(int x) { return (x >= 0 && x <= 59); }
};

struct SecondMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_SECOND;
  }

  static bool IsValid(int x) { return (x >= 0 && x <= 60); }
};

struct DecimalMatch : public NonterminalMatch {
  double value;
  int8 count_of_digits;  // When expression is in digits format.

  void Reset() {
    NonterminalMatch::Reset();
    value = NO_VAL;
    count_of_digits = 0;
  }
};

struct FractionSecondMatch : public DecimalMatch {
  void Reset() {
    DecimalMatch::Reset();
    type = MatchType::MatchType_FRACTION_SECOND;
  }

  static bool IsValid(double x) { return (x >= 0.0 && x < 1.0); }
};

// CombinedIntegersMatch<N> is used for expressions containing multiple (up
// to N) matches of integers without delimeters between them (because
// CFG-grammar is based on tokenizer, it could not split a token into several
// pieces like using regular-expression). For example, "1130" contains "11"
// and "30" meaning November 30.
template <int N>
struct CombinedIntegersMatch : public NonterminalMatch {
  enum {
    SIZE = N,
  };

  int values[SIZE];
  int8 count_of_digits;   // When expression is in digits format.
  bool is_zero_prefixed;  // When expression is in digits format.

  void Reset() {
    NonterminalMatch::Reset();
    for (int i = 0; i < SIZE; ++i) {
      values[i] = NO_VAL;
    }
    count_of_digits = 0;
    is_zero_prefixed = false;
  }
};

struct CombinedDigitsMatch : public CombinedIntegersMatch<6> {
  enum Index {
    INDEX_YEAR = 0,
    INDEX_MONTH = 1,
    INDEX_DAY = 2,
    INDEX_HOUR = 3,
    INDEX_MINUTE = 4,
    INDEX_SECOND = 5,
  };

  bool HasYear() const { return values[INDEX_YEAR] != NO_VAL; }
  bool HasMonth() const { return values[INDEX_MONTH] != NO_VAL; }
  bool HasDay() const { return values[INDEX_DAY] != NO_VAL; }
  bool HasHour() const { return values[INDEX_HOUR] != NO_VAL; }
  bool HasMinute() const { return values[INDEX_MINUTE] != NO_VAL; }
  bool HasSecond() const { return values[INDEX_SECOND] != NO_VAL; }

  int GetYear() const { return values[INDEX_YEAR]; }
  int GetMonth() const { return values[INDEX_MONTH]; }
  int GetDay() const { return values[INDEX_DAY]; }
  int GetHour() const { return values[INDEX_HOUR]; }
  int GetMinute() const { return values[INDEX_MINUTE]; }
  int GetSecond() const { return values[INDEX_SECOND]; }

  void Reset() {
    CombinedIntegersMatch<SIZE>::Reset();
    type = MatchType::MatchType_COMBINED_DIGITS;
  }

  static bool IsValid(int i, int x) {
    switch (i) {
      case INDEX_YEAR:
        return YearMatch::IsValid(x);
      case INDEX_MONTH:
        return MonthMatch::IsValid(x);
      case INDEX_DAY:
        return DayMatch::IsValid(x);
      case INDEX_HOUR:
        return HourMatch::IsValid(x);
      case INDEX_MINUTE:
        return MinuteMatch::IsValid(x);
      case INDEX_SECOND:
        return SecondMatch::IsValid(x);
      default:
        return false;
    }
  }
};

struct TimeValueMatch : public NonterminalMatch {
  const HourMatch* hour_match;
  const MinuteMatch* minute_match;
  const SecondMatch* second_match;
  const FractionSecondMatch* fraction_second_match;

  bool is_hour_zero_prefixed : 1;
  bool is_minute_one_digit : 1;
  bool is_second_one_digit : 1;

  int8 hour;
  int8 minute;
  int8 second;
  double fraction_second;

  void Reset() {
    NonterminalMatch::Reset();
    type = MatchType::MatchType_TIME_VALUE;
    hour_match = nullptr;
    minute_match = nullptr;
    second_match = nullptr;
    fraction_second_match = nullptr;
    is_hour_zero_prefixed = false;
    is_minute_one_digit = false;
    is_second_one_digit = false;
    hour = NO_VAL;
    minute = NO_VAL;
    second = NO_VAL;
    fraction_second = NO_VAL;
  }
};

struct TimeSpanMatch : public NonterminalMatch {
  const TimeSpanSpec* time_span_spec;
  TimespanCode time_span_code;

  void Reset() {
    NonterminalMatch::Reset();
    type = MatchType::MatchType_TIME_SPAN;
    time_span_spec = nullptr;
    time_span_code = TimespanCode_TIMESPAN_CODE_NONE;
  }
};

struct TimeZoneNameMatch : public NonterminalMatch {
  const TimeZoneNameSpec* time_zone_name_spec;
  TimezoneCode time_zone_code;

  void Reset() {
    NonterminalMatch::Reset();
    type = MatchType::MatchType_TIME_ZONE_NAME;
    time_zone_name_spec = nullptr;
    time_zone_code = TimezoneCode_TIMEZONE_CODE_NONE;
  }
};

struct TimeZoneOffsetMatch : public NonterminalMatch {
  const TimeZoneOffsetParameter* time_zone_offset_param;
  int16 time_zone_offset;

  void Reset() {
    NonterminalMatch::Reset();
    type = MatchType::MatchType_TIME_ZONE_OFFSET;
    time_zone_offset_param = nullptr;
    time_zone_offset = 0;
  }
};

struct DayOfWeekMatch : public IntegerMatch {
  void Reset() {
    IntegerMatch::Reset();
    type = MatchType::MatchType_DAY_OF_WEEK;
  }

  static bool IsValid(int x) {
    return (x > DayOfWeek_DOW_NONE && x <= DayOfWeek_MAX);
  }
};

struct TimePeriodMatch : public NonterminalMatch {
  int value;

  void Reset() {
    NonterminalMatch::Reset();
    type = MatchType::MatchType_TIME_PERIOD;
    value = NO_VAL;
  }
};

struct RelativeMatch : public NonterminalMatch {
  enum {
    HAS_NONE = 0,
    HAS_YEAR = 1 << 0,
    HAS_MONTH = 1 << 1,
    HAS_DAY = 1 << 2,
    HAS_WEEK = 1 << 3,
    HAS_HOUR = 1 << 4,
    HAS_MINUTE = 1 << 5,
    HAS_SECOND = 1 << 6,
    HAS_DAY_OF_WEEK = 1 << 7,
    HAS_IS_FUTURE = 1 << 31,
  };
  uint32 existing;

  int year;
  int month;
  int day;
  int week;
  int hour;
  int minute;
  int second;
  const NonterminalValue* day_of_week_nonterminal;
  int8 day_of_week;
  bool is_future_date;

  bool HasDay() const { return existing & HAS_DAY; }

  bool HasDayFields() const { return existing & (HAS_DAY | HAS_DAY_OF_WEEK); }

  bool HasTimeValueFields() const {
    return existing & (HAS_HOUR | HAS_MINUTE | HAS_SECOND);
  }

  bool IsStandaloneRelativeDayOfWeek() const {
    return (existing & HAS_DAY_OF_WEEK) && (existing & ~HAS_DAY_OF_WEEK) == 0;
  }

  void Reset() {
    NonterminalMatch::Reset();
    type = MatchType::MatchType_RELATIVE_DATE;
    existing = HAS_NONE;
    year = NO_VAL;
    month = NO_VAL;
    day = NO_VAL;
    week = NO_VAL;
    hour = NO_VAL;
    minute = NO_VAL;
    second = NO_VAL;
    day_of_week = NO_VAL;
    is_future_date = false;
  }
};

// This is not necessarily POD, it is used to keep the final matched result.
struct DateMatch {
  // Sub-matches in the date match.
  const YearMatch* year_match = nullptr;
  const MonthMatch* month_match = nullptr;
  const DayMatch* day_match = nullptr;
  const DayOfWeekMatch* day_of_week_match = nullptr;
  const TimeValueMatch* time_value_match = nullptr;
  const TimeSpanMatch* time_span_match = nullptr;
  const TimeZoneNameMatch* time_zone_name_match = nullptr;
  const TimeZoneOffsetMatch* time_zone_offset_match = nullptr;
  const RelativeMatch* relative_match = nullptr;
  const CombinedDigitsMatch* combined_digits_match = nullptr;

  // [begin, end) indicates the Document position where the date or date range
  // was found.
  int begin = -1;
  int end = -1;
  int priority = 0;
  float annotator_priority_score = 0.0;

  int year = NO_VAL;
  int8 month = NO_VAL;
  int8 day = NO_VAL;
  DayOfWeek day_of_week = DayOfWeek_DOW_NONE;
  BCAD bc_ad = BCAD_BCAD_NONE;
  int8 hour = NO_VAL;
  int8 minute = NO_VAL;
  int8 second = NO_VAL;
  double fraction_second = NO_VAL;
  TimespanCode time_span_code = TimespanCode_TIMESPAN_CODE_NONE;
  int time_zone_code = TimezoneCode_TIMEZONE_CODE_NONE;
  int16 time_zone_offset = std::numeric_limits<int16>::min();

  // Fields about ambiguous hours. These fields are used to interpret the
  // possible values of ambiguous hours. Since all kinds of known ambiguities
  // are in the form of arithmetic progression (starting from .hour field),
  // we can use "ambiguous_hour_count" to denote the count of ambiguous hours,
  // and use "ambiguous_hour_interval" to denote the distance between a pair
  // of adjacent possible hours. Values in the arithmetic progression are
  // shrunk into [0, 23] (MOD 24). One can use the GetPossibleHourValues()
  // method for the complete list of possible hours.
  uint8 ambiguous_hour_count = 0;
  uint8 ambiguous_hour_interval = 0;

  bool is_inferred = false;

  // This field is set in function PerformRefinements to remove some DateMatch
  // like overlapped, duplicated, etc.
  bool is_removed = false;

  std::string DebugString() const;

  bool HasYear() const { return year != NO_VAL; }
  bool HasMonth() const { return month != NO_VAL; }
  bool HasDay() const { return day != NO_VAL; }
  bool HasDayOfWeek() const { return day_of_week != DayOfWeek_DOW_NONE; }
  bool HasBcAd() const { return bc_ad != BCAD_BCAD_NONE; }
  bool HasHour() const { return hour != NO_VAL; }
  bool HasMinute() const { return minute != NO_VAL; }
  bool HasSecond() const { return second != NO_VAL; }
  bool HasFractionSecond() const { return fraction_second != NO_VAL; }
  bool HasTimeSpanCode() const {
    return time_span_code != TimespanCode_TIMESPAN_CODE_NONE;
  }
  bool HasTimeZoneCode() const {
    return time_zone_code != TimezoneCode_TIMEZONE_CODE_NONE;
  }
  bool HasTimeZoneOffset() const {
    return time_zone_offset != std::numeric_limits<int16>::min();
  }

  bool HasRelativeDate() const { return relative_match != nullptr; }

  bool IsHourAmbiguous() const { return ambiguous_hour_count >= 2; }

  bool IsStandaloneTime() const {
    return (HasHour() || HasMinute()) && !HasDayOfWeek() && !HasDay() &&
           !HasMonth() && !HasYear();
  }

  void SetAmbiguousHourProperties(uint8 count, uint8 interval) {
    ambiguous_hour_count = count;
    ambiguous_hour_interval = interval;
  }

  // Outputs all the possible hour values. If current DateMatch does not
  // contain an hour, nothing will be output. If the hour is not ambiguous,
  // only one value (= .hour) will be output. This method clears the vector
  // "values" first, and it is not guaranteed that the values in the vector
  // are in a sorted order.
  void GetPossibleHourValues(std::vector<int8>* values) const;

  int GetPriority() const { return priority; }

  float GetAnnotatorPriorityScore() const { return annotator_priority_score; }

  bool IsStandaloneRelativeDayOfWeek() const {
    return (HasRelativeDate() &&
            relative_match->IsStandaloneRelativeDayOfWeek() &&
            !HasDateFields() && !HasTimeFields() && !HasTimeSpanCode());
  }

  bool HasDateFields() const {
    return (HasYear() || HasMonth() || HasDay() || HasDayOfWeek() || HasBcAd());
  }
  bool HasTimeValueFields() const {
    return (HasHour() || HasMinute() || HasSecond() || HasFractionSecond());
  }
  bool HasTimeSpanFields() const { return HasTimeSpanCode(); }
  bool HasTimeZoneFields() const {
    return (HasTimeZoneCode() || HasTimeZoneOffset());
  }
  bool HasTimeFields() const {
    return (HasTimeValueFields() || HasTimeSpanFields() || HasTimeZoneFields());
  }

  bool IsValid() const;

  // Overall relative qualifier of the DateMatch e.g. 2 year ago is 'PAST' and
  // next week is 'FUTURE'.
  DatetimeComponent::RelativeQualifier GetRelativeQualifier() const;

  // Getter method to get the 'DatetimeComponent' of given 'ComponentType'.
  Optional<DatetimeComponent> GetDatetimeComponent(
      const DatetimeComponent::ComponentType& component_type) const;

  void FillDatetimeComponents(
      std::vector<DatetimeComponent>* datetime_component) const;
};

// Represent a matched date range which includes the from and to matched date.
struct DateRangeMatch {
  int begin = -1;
  int end = -1;

  DateMatch from;
  DateMatch to;

  std::string DebugString() const;

  int GetPriority() const {
    return std::max(from.GetPriority(), to.GetPriority());
  }

  float GetAnnotatorPriorityScore() const {
    return std::max(from.GetAnnotatorPriorityScore(),
                    to.GetAnnotatorPriorityScore());
  }
};

}  // namespace dates
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_DATE_MATCH_H_
