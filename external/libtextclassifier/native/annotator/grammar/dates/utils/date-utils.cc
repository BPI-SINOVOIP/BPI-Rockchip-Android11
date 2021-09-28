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

#include "annotator/grammar/dates/utils/date-utils.h"

#include <algorithm>
#include <ctime>

#include "annotator/grammar/dates/annotations/annotation-util.h"
#include "annotator/grammar/dates/dates_generated.h"
#include "annotator/grammar/dates/utils/annotation-keys.h"
#include "annotator/grammar/dates/utils/date-match.h"
#include "annotator/types.h"
#include "utils/base/macros.h"

namespace libtextclassifier3 {
namespace dates {

bool IsLeapYear(int year) {
  // For the sake of completeness, we want to be able to decide
  // whether a year is a leap year all the way back to 0 Julian, or
  // 4714 BCE. But we don't want to take the modulus of a negative
  // number, because this may not be very well-defined or portable. So
  // we increment the year by some large multiple of 400, which is the
  // periodicity of this leap-year calculation.
  if (year < 0) {
    year += 8000;
  }
  return ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0));
}

namespace {
#define SECSPERMIN (60)
#define MINSPERHOUR (60)
#define HOURSPERDAY (24)
#define DAYSPERWEEK (7)
#define DAYSPERNYEAR (365)
#define DAYSPERLYEAR (366)
#define MONSPERYEAR (12)

const int8 kDaysPerMonth[2][1 + MONSPERYEAR] = {
    {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {-1, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};
}  // namespace

int8 GetLastDayOfMonth(int year, int month) {
  if (year == 0) {  // No year specified
    return kDaysPerMonth[1][month];
  }
  return kDaysPerMonth[IsLeapYear(year)][month];
}

namespace {
inline bool IsHourInSegment(const TimeSpanSpec_::Segment* segment, int8 hour,
                            bool is_exact) {
  return (hour >= segment->begin() &&
          (hour < segment->end() ||
           (hour == segment->end() && is_exact && segment->is_closed())));
}

Property* FindOrCreateDefaultDateTime(AnnotationData* inst) {
  // Refer comments for kDateTime in annotation-keys.h to see the format.
  static constexpr int kDefault[] = {-1, -1, -1, -1, -1, -1, -1, -1};

  int idx = GetPropertyIndex(kDateTime, *inst);
  if (idx < 0) {
    idx = AddRepeatedIntProperty(kDateTime, kDefault, TC3_ARRAYSIZE(kDefault),
                                 inst);
  }
  return &inst->properties[idx];
}

void IncrementDayOfWeek(DayOfWeek* dow) {
  static const DayOfWeek dow_ring[] = {DayOfWeek_MONDAY,    DayOfWeek_TUESDAY,
                                       DayOfWeek_WEDNESDAY, DayOfWeek_THURSDAY,
                                       DayOfWeek_FRIDAY,    DayOfWeek_SATURDAY,
                                       DayOfWeek_SUNDAY,    DayOfWeek_MONDAY};
  const auto& cur_dow =
      std::find(std::begin(dow_ring), std::end(dow_ring), *dow);
  if (cur_dow != std::end(dow_ring)) {
    *dow = *std::next(cur_dow);
  }
}
}  // namespace

bool NormalizeHourByTimeSpan(const TimeSpanSpec* ts_spec, DateMatch* date) {
  if (ts_spec->segment() == nullptr) {
    return false;
  }
  if (date->HasHour()) {
    const bool is_exact =
        (!date->HasMinute() ||
         (date->minute == 0 &&
          (!date->HasSecond() ||
           (date->second == 0 &&
            (!date->HasFractionSecond() || date->fraction_second == 0.0)))));
    for (const TimeSpanSpec_::Segment* segment : *ts_spec->segment()) {
      if (IsHourInSegment(segment, date->hour + segment->offset(), is_exact)) {
        date->hour += segment->offset();
        return true;
      }
      if (!segment->is_strict() &&
          IsHourInSegment(segment, date->hour, is_exact)) {
        return true;
      }
    }
  } else {
    for (const TimeSpanSpec_::Segment* segment : *ts_spec->segment()) {
      if (segment->is_stand_alone()) {
        if (segment->begin() == segment->end()) {
          date->hour = segment->begin();
        }
        // Allow stand-alone time-span points and ranges.
        return true;
      }
    }
  }
  return false;
}

bool IsRefinement(const DateMatch& a, const DateMatch& b) {
  int count = 0;
  if (b.HasBcAd()) {
    if (!a.HasBcAd() || a.bc_ad != b.bc_ad) return false;
  } else if (a.HasBcAd()) {
    if (a.bc_ad == BCAD_BC) return false;
    ++count;
  }
  if (b.HasYear()) {
    if (!a.HasYear() || a.year != b.year) return false;
  } else if (a.HasYear()) {
    ++count;
  }
  if (b.HasMonth()) {
    if (!a.HasMonth() || a.month != b.month) return false;
  } else if (a.HasMonth()) {
    ++count;
  }
  if (b.HasDay()) {
    if (!a.HasDay() || a.day != b.day) return false;
  } else if (a.HasDay()) {
    ++count;
  }
  if (b.HasDayOfWeek()) {
    if (!a.HasDayOfWeek() || a.day_of_week != b.day_of_week) return false;
  } else if (a.HasDayOfWeek()) {
    ++count;
  }
  if (b.HasHour()) {
    if (!a.HasHour()) return false;
    std::vector<int8> possible_hours;
    b.GetPossibleHourValues(&possible_hours);
    if (std::find(possible_hours.begin(), possible_hours.end(), a.hour) ==
        possible_hours.end()) {
      return false;
    }
  } else if (a.HasHour()) {
    ++count;
  }
  if (b.HasMinute()) {
    if (!a.HasMinute() || a.minute != b.minute) return false;
  } else if (a.HasMinute()) {
    ++count;
  }
  if (b.HasSecond()) {
    if (!a.HasSecond() || a.second != b.second) return false;
  } else if (a.HasSecond()) {
    ++count;
  }
  if (b.HasFractionSecond()) {
    if (!a.HasFractionSecond() || a.fraction_second != b.fraction_second)
      return false;
  } else if (a.HasFractionSecond()) {
    ++count;
  }
  if (b.HasTimeSpanCode()) {
    if (!a.HasTimeSpanCode() || a.time_span_code != b.time_span_code)
      return false;
  } else if (a.HasTimeSpanCode()) {
    ++count;
  }
  if (b.HasTimeZoneCode()) {
    if (!a.HasTimeZoneCode() || a.time_zone_code != b.time_zone_code)
      return false;
  } else if (a.HasTimeZoneCode()) {
    ++count;
  }
  if (b.HasTimeZoneOffset()) {
    if (!a.HasTimeZoneOffset() || a.time_zone_offset != b.time_zone_offset)
      return false;
  } else if (a.HasTimeZoneOffset()) {
    ++count;
  }
  return (count > 0 || a.priority >= b.priority);
}

bool IsRefinement(const DateRangeMatch& a, const DateRangeMatch& b) {
  return false;
}

bool IsPrecedent(const DateMatch& a, const DateMatch& b) {
  if (a.HasYear() && b.HasYear()) {
    if (a.year < b.year) return true;
    if (a.year > b.year) return false;
  }

  if (a.HasMonth() && b.HasMonth()) {
    if (a.month < b.month) return true;
    if (a.month > b.month) return false;
  }

  if (a.HasDay() && b.HasDay()) {
    if (a.day < b.day) return true;
    if (a.day > b.day) return false;
  }

  if (a.HasHour() && b.HasHour()) {
    if (a.hour < b.hour) return true;
    if (a.hour > b.hour) return false;
  }

  if (a.HasMinute() && b.HasHour()) {
    if (a.minute < b.hour) return true;
    if (a.minute > b.hour) return false;
  }

  if (a.HasSecond() && b.HasSecond()) {
    if (a.second < b.hour) return true;
    if (a.second > b.hour) return false;
  }

  return false;
}

void FillDateInstance(const DateMatch& date,
                      DatetimeParseResultSpan* instance) {
  instance->span.first = date.begin;
  instance->span.second = date.end;
  instance->priority_score = date.GetAnnotatorPriorityScore();
  DatetimeParseResult datetime_parse_result;
  date.FillDatetimeComponents(&datetime_parse_result.datetime_components);
  instance->data.emplace_back(datetime_parse_result);
}

void FillDateRangeInstance(const DateRangeMatch& range,
                           DatetimeParseResultSpan* instance) {
  instance->span.first = range.begin;
  instance->span.second = range.end;
  instance->priority_score = range.GetAnnotatorPriorityScore();

  // Filling from DatetimeParseResult.
  instance->data.emplace_back();
  range.from.FillDatetimeComponents(&instance->data.back().datetime_components);

  // Filling to DatetimeParseResult.
  instance->data.emplace_back();
  range.to.FillDatetimeComponents(&instance->data.back().datetime_components);
}

namespace {
bool AnyOverlappedField(const DateMatch& prev, const DateMatch& next) {
#define Field(f) \
  if (prev.f && next.f) return true
  Field(year_match);
  Field(month_match);
  Field(day_match);
  Field(day_of_week_match);
  Field(time_value_match);
  Field(time_span_match);
  Field(time_zone_name_match);
  Field(time_zone_offset_match);
  Field(relative_match);
  Field(combined_digits_match);
#undef Field
  return false;
}

void MergeDateMatchImpl(const DateMatch& prev, DateMatch* next,
                        bool update_span) {
#define RM(f) \
  if (!next->f) next->f = prev.f
  RM(year_match);
  RM(month_match);
  RM(day_match);
  RM(day_of_week_match);
  RM(time_value_match);
  RM(time_span_match);
  RM(time_zone_name_match);
  RM(time_zone_offset_match);
  RM(relative_match);
  RM(combined_digits_match);
#undef RM

#define RV(f) \
  if (next->f == NO_VAL) next->f = prev.f
  RV(year);
  RV(month);
  RV(day);
  RV(hour);
  RV(minute);
  RV(second);
  RV(fraction_second);
#undef RV

#define RE(f, v) \
  if (next->f == v) next->f = prev.f
  RE(day_of_week, DayOfWeek_DOW_NONE);
  RE(bc_ad, BCAD_BCAD_NONE);
  RE(time_span_code, TimespanCode_TIMESPAN_CODE_NONE);
  RE(time_zone_code, TimezoneCode_TIMEZONE_CODE_NONE);
#undef RE

  if (next->time_zone_offset == std::numeric_limits<int16>::min()) {
    next->time_zone_offset = prev.time_zone_offset;
  }

  next->priority = std::max(next->priority, prev.priority);
  next->annotator_priority_score =
      std::max(next->annotator_priority_score, prev.annotator_priority_score);
  if (update_span) {
    next->begin = std::min(next->begin, prev.begin);
    next->end = std::max(next->end, prev.end);
  }
}
}  // namespace

bool IsDateMatchMergeable(const DateMatch& prev, const DateMatch& next) {
  // Do not merge if they share the same field.
  if (AnyOverlappedField(prev, next)) {
    return false;
  }

  // It's impossible that both prev and next have relative date since it's
  // excluded by overlapping check before.
  if (prev.HasRelativeDate() || next.HasRelativeDate()) {
    // If one of them is relative date, then we merge:
    //   - if relative match shouldn't have time, and always has DOW or day.
    //   - if not both relative match and non relative match has day.
    //   - if non relative match has time or day.
    const DateMatch* rm = &prev;
    const DateMatch* non_rm = &prev;
    if (prev.HasRelativeDate()) {
      non_rm = &next;
    } else {
      rm = &next;
    }

    const RelativeMatch* relative_match = rm->relative_match;
    // Relative Match should have day or DOW but no time.
    if (!relative_match->HasDayFields() ||
        relative_match->HasTimeValueFields()) {
      return false;
    }
    // Check if both relative match and non relative match has day.
    if (non_rm->HasDateFields() && relative_match->HasDay()) {
      return false;
    }
    // Non relative match should have either hour (time) or day (date).
    if (!non_rm->HasHour() && !non_rm->HasDay()) {
      return false;
    }
  } else {
    // Only one match has date and another has time.
    if ((prev.HasDateFields() && next.HasDateFields()) ||
        (prev.HasTimeFields() && next.HasTimeFields())) {
      return false;
    }
    // DOW never be extracted as a single DateMatch except in RelativeMatch. So
    // here, we always merge one with day and another one with hour.
    if (!(prev.HasDay() || next.HasDay()) ||
        !(prev.HasHour() || next.HasHour())) {
      return false;
    }
  }
  return true;
}

void MergeDateMatch(const DateMatch& prev, DateMatch* next, bool update_span) {
  if (IsDateMatchMergeable(prev, *next)) {
    MergeDateMatchImpl(prev, next, update_span);
  }
}

}  // namespace dates
}  // namespace libtextclassifier3
