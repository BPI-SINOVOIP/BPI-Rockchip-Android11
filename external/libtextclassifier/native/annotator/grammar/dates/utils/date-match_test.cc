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

#include <stdint.h>

#include <string>

#include "annotator/grammar/dates/dates_generated.h"
#include "annotator/grammar/dates/timezone-code_generated.h"
#include "annotator/grammar/dates/utils/date-utils.h"
#include "utils/strings/append.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace dates {
namespace {

class DateMatchTest : public ::testing::Test {
 protected:
  enum {
    X = NO_VAL,
  };

  static DayOfWeek DOW_X() { return DayOfWeek_DOW_NONE; }
  static DayOfWeek SUN() { return DayOfWeek_SUNDAY; }

  static BCAD BCAD_X() { return BCAD_BCAD_NONE; }
  static BCAD BC() { return BCAD_BC; }

  DateMatch& SetDate(DateMatch* date, int year, int8 month, int8 day,
                     DayOfWeek day_of_week = DOW_X(), BCAD bc_ad = BCAD_X()) {
    date->year = year;
    date->month = month;
    date->day = day;
    date->day_of_week = day_of_week;
    date->bc_ad = bc_ad;
    return *date;
  }

  DateMatch& SetTimeValue(DateMatch* date, int8 hour, int8 minute = X,
                          int8 second = X, double fraction_second = X) {
    date->hour = hour;
    date->minute = minute;
    date->second = second;
    date->fraction_second = fraction_second;
    return *date;
  }

  DateMatch& SetTimeSpan(DateMatch* date, TimespanCode time_span_code) {
    date->time_span_code = time_span_code;
    return *date;
  }

  DateMatch& SetTimeZone(DateMatch* date, TimezoneCode time_zone_code,
                         int16 time_zone_offset = INT16_MIN) {
    date->time_zone_code = time_zone_code;
    date->time_zone_offset = time_zone_offset;
    return *date;
  }

  bool SameDate(const DateMatch& a, const DateMatch& b) {
    return (a.day == b.day && a.month == b.month && a.year == b.year &&
            a.day_of_week == b.day_of_week);
  }

  DateMatch& SetDayOfWeek(DateMatch* date, DayOfWeek dow) {
    date->day_of_week = dow;
    return *date;
  }
};

TEST_F(DateMatchTest, BitFieldWidth) {
  // For DateMatch::day_of_week (:8).
  EXPECT_GE(DayOfWeek_MIN, INT8_MIN);
  EXPECT_LE(DayOfWeek_MAX, INT8_MAX);

  // For DateMatch::bc_ad (:8).
  EXPECT_GE(BCAD_MIN, INT8_MIN);
  EXPECT_LE(BCAD_MAX, INT8_MAX);

  // For DateMatch::time_span_code (:16).
  EXPECT_GE(TimespanCode_MIN, INT16_MIN);
  EXPECT_LE(TimespanCode_MAX, INT16_MAX);
}

TEST_F(DateMatchTest, IsValid) {
  // Valid: dates.
  {
    DateMatch d;
    SetDate(&d, 2014, 1, 26);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, 2014, 1, X);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, 2014, X, X);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, 1, 26);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, 1, X);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, X, 26);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, 2014, 1, 26, SUN());
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, 1, 26, SUN());
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, X, 26, SUN());
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, 2014, 1, 26, DOW_X(), BC());
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  // Valid: times.
  {
    DateMatch d;
    SetTimeValue(&d, 12, 30, 59, 0.99);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetTimeValue(&d, 12, 30, 59);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetTimeValue(&d, 12, 30);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetTimeValue(&d, 12);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  // Valid: mixed.
  {
    DateMatch d;
    SetDate(&d, 2014, 1, 26);
    SetTimeValue(&d, 12, 30, 59, 0.99);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, 1, 26);
    SetTimeValue(&d, 12, 30, 59);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, X, X, SUN());
    SetTimeValue(&d, 12, 30);
    EXPECT_TRUE(d.IsValid()) << d.DebugString();
  }
  // Invalid: dates.
  {
    DateMatch d;
    SetDate(&d, X, 1, 26, DOW_X(), BC());
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, 2014, X, 26);
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, 2014, X, X, SUN());
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetDate(&d, X, 1, X, SUN());
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  // Invalid: times.
  {
    DateMatch d;
    SetTimeValue(&d, 12, X, 59);
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetTimeValue(&d, 12, X, X, 0.99);
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetTimeValue(&d, 12, 30, X, 0.99);
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  {
    DateMatch d;
    SetTimeValue(&d, X, 30);
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  // Invalid: mixed.
  {
    DateMatch d;
    SetDate(&d, 2014, 1, X);
    SetTimeValue(&d, 12);
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
  // Invalid: empty.
  {
    DateMatch d;
    EXPECT_FALSE(d.IsValid()) << d.DebugString();
  }
}

std::string DebugStrings(const std::vector<DateMatch>& instances) {
  std::string res;
  for (int i = 0; i < instances.size(); ++i) {
    ::libtextclassifier3::strings::SStringAppendF(
        &res, 0, "[%d] == %s\n", i, instances[i].DebugString().c_str());
  }
  return res;
}

TEST_F(DateMatchTest, IsRefinement) {
  {
    DateMatch a;
    SetDate(&a, 2014, 2, X);
    DateMatch b;
    SetDate(&b, 2014, X, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    DateMatch b;
    SetDate(&b, 2014, 2, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    DateMatch b;
    SetDate(&b, X, 2, 24);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, X, X);
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, 0, X);
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    SetTimeValue(&b, 9, X, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, 0, 0);
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    SetTimeValue(&b, 9, 0, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, X, X);
    SetTimeSpan(&a, TimespanCode_AM);
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    SetTimeValue(&b, 9, X, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, X, X);
    SetTimeZone(&a, TimezoneCode_PST8PDT);
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    SetTimeValue(&b, 9, X, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, X, X);
    a.priority += 10;
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    SetTimeValue(&b, 9, X, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, X, X);
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    SetTimeValue(&b, 9, X, X);
    EXPECT_TRUE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, 2014, 2, 24);
    SetTimeValue(&a, 9, X, X);
    DateMatch b;
    SetDate(&b, X, 2, 24);
    SetTimeValue(&b, 9, 0, X);
    EXPECT_FALSE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetDate(&a, X, 2, 24);
    SetTimeValue(&a, 9, X, X);
    DateMatch b;
    SetDate(&b, 2014, 2, 24);
    EXPECT_FALSE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
  {
    DateMatch a;
    SetTimeValue(&a, 9, 0, 0);
    DateMatch b;
    SetTimeValue(&b, 9, X, X);
    SetTimeSpan(&b, TimespanCode_AM);
    EXPECT_FALSE(IsRefinement(a, b)) << DebugStrings({a, b});
  }
}

TEST_F(DateMatchTest, FillDateInstance_AnnotatorPriorityScore) {
  DateMatch date_match;
  SetDate(&date_match, 2014, 2, X);
  date_match.annotator_priority_score = 0.5;
  DatetimeParseResultSpan datetime_parse_result_span;
  FillDateInstance(date_match, &datetime_parse_result_span);
  EXPECT_FLOAT_EQ(datetime_parse_result_span.priority_score, 0.5)
      << DebugStrings({date_match});
}

TEST_F(DateMatchTest, MergeDateMatch_AnnotatorPriorityScore) {
  DateMatch a;
  SetDate(&a, 2014, 2, 4);
  a.annotator_priority_score = 0.5;

  DateMatch b;
  SetTimeValue(&b, 10, 45, 23);
  b.annotator_priority_score = 1.0;

  MergeDateMatch(b, &a, false);
  EXPECT_FLOAT_EQ(a.annotator_priority_score, 1.0);
}

}  // namespace
}  // namespace dates
}  // namespace libtextclassifier3
