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

#ifndef LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_COMMON_H_
#define LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_COMMON_H_

#include "annotator/types.h"
#include "utils/base/integral_types.h"
#include "utils/base/logging.h"
#include "utils/base/macros.h"

namespace libtextclassifier3 {
namespace calendar {

// Macro to reduce the amount of boilerplate needed for propagating errors.
#define TC3_CALENDAR_CHECK(EXPR) \
  if (!(EXPR)) {                 \
    return false;                \
  }

// An implementation of CalendarLib that is independent of the particular
// calendar implementation used (implementation type is passed as template
// argument).
template <class TCalendar>
class CalendarLibTempl {
 public:
  bool InterpretParseData(const DatetimeParsedData& parse_data,
                          int64 reference_time_ms_utc,
                          const std::string& reference_timezone,
                          const std::string& reference_locale,
                          bool prefer_future_for_unspecified_date,
                          TCalendar* calendar,
                          DatetimeGranularity* granularity) const;

  DatetimeGranularity GetGranularity(const DatetimeParsedData& data) const;

 private:
  // Adjusts the calendar's time instant according to a relative date reference
  // in the parsed data.
  bool ApplyRelationField(const DatetimeComponent& relative_date_time_component,
                          TCalendar* calendar) const;

  // Round the time instant's precision down to the given granularity.
  bool RoundToGranularity(DatetimeGranularity granularity,
                          TCalendar* calendar) const;

  // Adjusts time in steps of relation_type, by distance steps.
  // For example:
  // - Adjusting by -2 MONTHS will return the beginning of the 1st
  //   two weeks ago.
  // - Adjusting by +4 Wednesdays will return the beginning of the next
  //   Wednesday at least 4 weeks from now.
  // If allow_today is true, the same day of the week may be kept
  // if it already matches the relation type.
  bool AdjustByRelation(DatetimeComponent date_time_component, int distance,
                        bool allow_today, TCalendar* calendar) const;
};

inline bool HasOnlyTimeComponents(const DatetimeParsedData& parse_data) {
  std::vector<DatetimeComponent> components;
  parse_data.GetDatetimeComponents(&components);

  for (const DatetimeComponent& component : components) {
    if (!(component.component_type == DatetimeComponent::ComponentType::HOUR ||
          component.component_type ==
              DatetimeComponent::ComponentType::MINUTE ||
          component.component_type ==
              DatetimeComponent::ComponentType::SECOND ||
          component.component_type ==
              DatetimeComponent::ComponentType::MERIDIEM)) {
      return false;
    }
  }
  return true;
}

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::InterpretParseData(
    const DatetimeParsedData& parse_data, int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::string& reference_locale,
    bool prefer_future_for_unspecified_date, TCalendar* calendar,
    DatetimeGranularity* granularity) const {
  TC3_CALENDAR_CHECK(calendar->Initialize(reference_timezone, reference_locale,
                                          reference_time_ms_utc))

  bool should_round_to_granularity = true;
  *granularity = GetGranularity(parse_data);

  // Apply each of the parsed fields in order of increasing granularity.
  static const int64 kMillisInMinute = 1000 * 60;
  if (parse_data.HasFieldType(DatetimeComponent::ComponentType::ZONE_OFFSET)) {
    int zone_offset;
    parse_data.GetFieldValue(DatetimeComponent::ComponentType::ZONE_OFFSET,
                             &zone_offset);
    TC3_CALENDAR_CHECK(calendar->SetZoneOffset(zone_offset * kMillisInMinute))
  }
  static const int64 kMillisInHour = 1000 * 60 * 60;
  if (parse_data.HasFieldType(DatetimeComponent::ComponentType::DST_OFFSET)) {
    int dst_offset;
    if (parse_data.GetFieldValue(DatetimeComponent::ComponentType::DST_OFFSET,
                                 &dst_offset)) {
      TC3_CALENDAR_CHECK(calendar->SetDstOffset(dst_offset * kMillisInHour))
    }
  }
  std::vector<DatetimeComponent> relative_components;
  parse_data.GetRelativeDatetimeComponents(&relative_components);
  if (!relative_components.empty()) {
    // Currently only one relative date time component is possible.
    const DatetimeComponent& relative_component = relative_components.back();
    TC3_CALENDAR_CHECK(ApplyRelationField(relative_component, calendar));
    should_round_to_granularity = relative_component.ShouldRoundToGranularity();
  } else {
    // By default, the parsed time is interpreted to be on the reference day.
    // But a parsed date should have time 0:00:00 unless specified.
    TC3_CALENDAR_CHECK(calendar->SetHourOfDay(0))
    TC3_CALENDAR_CHECK(calendar->SetMinute(0))
    TC3_CALENDAR_CHECK(calendar->SetSecond(0))
    TC3_CALENDAR_CHECK(calendar->SetMillisecond(0))
  }
  if (parse_data.HasAbsoluteValue(DatetimeComponent::ComponentType::YEAR)) {
    int year;
    parse_data.GetFieldValue(DatetimeComponent::ComponentType::YEAR, &year);
    TC3_CALENDAR_CHECK(calendar->SetYear(year))
  }
  if (parse_data.HasAbsoluteValue(DatetimeComponent::ComponentType::MONTH)) {
    int month;
    parse_data.GetFieldValue(DatetimeComponent::ComponentType::MONTH, &month);
    // ICU has months starting at 0, Java and Datetime parser at 1, so we
    // need to subtract 1.
    TC3_CALENDAR_CHECK(calendar->SetMonth(month - 1))
  }

  if (parse_data.HasAbsoluteValue(
          DatetimeComponent::ComponentType::DAY_OF_MONTH)) {
    int day_of_month;
    parse_data.GetFieldValue(DatetimeComponent::ComponentType::DAY_OF_MONTH,
                             &day_of_month);
    TC3_CALENDAR_CHECK(calendar->SetDayOfMonth(day_of_month))
  }
  if (parse_data.HasAbsoluteValue(DatetimeComponent::ComponentType::HOUR)) {
    int hour;
    parse_data.GetFieldValue(DatetimeComponent::ComponentType::HOUR, &hour);
    if (parse_data.HasFieldType(DatetimeComponent::ComponentType::MERIDIEM)) {
      int merdiem;
      parse_data.GetFieldValue(DatetimeComponent::ComponentType::MERIDIEM,
                               &merdiem);
      if (merdiem == 1 && hour < 12) {
        TC3_CALENDAR_CHECK(calendar->SetHourOfDay(hour + 12))
      } else if (merdiem == 0 && hour == 12) {
        // Set hour of the day's value to zero (12am == 0:00 in 24 hour format).
        // Please see issue b/139923083.
        TC3_CALENDAR_CHECK(calendar->SetHourOfDay(0));
      } else {
        TC3_CALENDAR_CHECK(calendar->SetHourOfDay(hour))
      }
    } else {
      TC3_CALENDAR_CHECK(calendar->SetHourOfDay(hour))
    }
  }
  if (parse_data.HasAbsoluteValue(DatetimeComponent::ComponentType::MINUTE)) {
    int minute;
    parse_data.GetFieldValue(DatetimeComponent::ComponentType::MINUTE, &minute);
    TC3_CALENDAR_CHECK(calendar->SetMinute(minute))
  }
  if (parse_data.HasAbsoluteValue(DatetimeComponent::ComponentType::SECOND)) {
    int second;
    parse_data.GetFieldValue(DatetimeComponent::ComponentType::SECOND, &second);
    TC3_CALENDAR_CHECK(calendar->SetSecond(second))
  }
  if (should_round_to_granularity) {
    TC3_CALENDAR_CHECK(RoundToGranularity(*granularity, calendar))
  }

  int64 calendar_millis;
  TC3_CALENDAR_CHECK(calendar->GetTimeInMillis(&calendar_millis))
  if (prefer_future_for_unspecified_date &&
      calendar_millis < reference_time_ms_utc &&
      HasOnlyTimeComponents(parse_data)) {
    calendar->AddDayOfMonth(1);
  }

  return true;
}

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::ApplyRelationField(
    const DatetimeComponent& relative_date_time_component,
    TCalendar* calendar) const {
  switch (relative_date_time_component.relative_qualifier) {
    case DatetimeComponent::RelativeQualifier::UNSPECIFIED:
      TC3_LOG(ERROR) << "UNSPECIFIED RelationType.";
      return false;
    case DatetimeComponent::RelativeQualifier::NEXT:
      TC3_CALENDAR_CHECK(AdjustByRelation(relative_date_time_component,
                                          /*distance=*/1,
                                          /*allow_today=*/false, calendar));
      return true;
    case DatetimeComponent::RelativeQualifier::THIS:
      TC3_CALENDAR_CHECK(AdjustByRelation(relative_date_time_component,
                                          /*distance=*/1,
                                          /*allow_today=*/true, calendar))
      return true;
    case DatetimeComponent::RelativeQualifier::LAST:
      TC3_CALENDAR_CHECK(AdjustByRelation(relative_date_time_component,
                                          /*distance=*/-1,
                                          /*allow_today=*/false, calendar))
      return true;
    case DatetimeComponent::RelativeQualifier::NOW:
      return true;  // NOOP
    case DatetimeComponent::RelativeQualifier::TOMORROW:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(1));
      return true;
    case DatetimeComponent::RelativeQualifier::YESTERDAY:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(-1));
      return true;
    case DatetimeComponent::RelativeQualifier::PAST:
      TC3_CALENDAR_CHECK(
          AdjustByRelation(relative_date_time_component,
                           relative_date_time_component.relative_count,
                           /*allow_today=*/false, calendar))
      return true;
    case DatetimeComponent::RelativeQualifier::FUTURE:
      TC3_CALENDAR_CHECK(
          AdjustByRelation(relative_date_time_component,
                           relative_date_time_component.relative_count,
                           /*allow_today=*/false, calendar))
      return true;
  }
  return false;
}

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::RoundToGranularity(
    DatetimeGranularity granularity, TCalendar* calendar) const {
  // Force recomputation before doing the rounding.
  int unused;
  TC3_CALENDAR_CHECK(calendar->GetDayOfWeek(&unused));

  switch (granularity) {
    case GRANULARITY_YEAR:
      TC3_CALENDAR_CHECK(calendar->SetMonth(0));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_MONTH:
      TC3_CALENDAR_CHECK(calendar->SetDayOfMonth(1));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_DAY:
      TC3_CALENDAR_CHECK(calendar->SetHourOfDay(0));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_HOUR:
      TC3_CALENDAR_CHECK(calendar->SetMinute(0));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_MINUTE:
      TC3_CALENDAR_CHECK(calendar->SetSecond(0));
      break;

    case GRANULARITY_WEEK:
      int first_day_of_week;
      TC3_CALENDAR_CHECK(calendar->GetFirstDayOfWeek(&first_day_of_week));
      TC3_CALENDAR_CHECK(calendar->SetDayOfWeek(first_day_of_week));
      TC3_CALENDAR_CHECK(calendar->SetHourOfDay(0));
      TC3_CALENDAR_CHECK(calendar->SetMinute(0));
      TC3_CALENDAR_CHECK(calendar->SetSecond(0));
      break;

    case GRANULARITY_UNKNOWN:
    case GRANULARITY_SECOND:
      break;
  }
  return true;
}

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::AdjustByRelation(
    DatetimeComponent date_time_component, int distance, bool allow_today,
    TCalendar* calendar) const {
  const int distance_sign = distance < 0 ? -1 : 1;
  switch (date_time_component.component_type) {
    case DatetimeComponent::ComponentType::DAY_OF_WEEK:
      if (!allow_today) {
        // If we're not including the same day as the reference, skip it.
        TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(distance_sign))
      }
      // Keep walking back until we hit the desired day of the week.
      while (distance != 0) {
        int day_of_week;
        TC3_CALENDAR_CHECK(calendar->GetDayOfWeek(&day_of_week))
        if (day_of_week == (date_time_component.value)) {
          distance += -distance_sign;
          if (distance == 0) break;
        }
        TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(distance_sign))
      }
      return true;
    case DatetimeComponent::ComponentType::SECOND:
      TC3_CALENDAR_CHECK(calendar->AddSecond(distance));
      return true;
    case DatetimeComponent::ComponentType::MINUTE:
      TC3_CALENDAR_CHECK(calendar->AddMinute(distance));
      return true;
    case DatetimeComponent::ComponentType::HOUR:
      TC3_CALENDAR_CHECK(calendar->AddHourOfDay(distance));
      return true;
    case DatetimeComponent::ComponentType::DAY_OF_MONTH:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(distance));
      return true;
    case DatetimeComponent::ComponentType::WEEK:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(7 * distance))
      TC3_CALENDAR_CHECK(calendar->SetDayOfWeek(1))
      return true;
    case DatetimeComponent::ComponentType::MONTH:
      TC3_CALENDAR_CHECK(calendar->AddMonth(distance))
      TC3_CALENDAR_CHECK(calendar->SetDayOfMonth(1))
      return true;
    case DatetimeComponent::ComponentType::YEAR:
      TC3_CALENDAR_CHECK(calendar->AddYear(distance))
      TC3_CALENDAR_CHECK(calendar->SetDayOfYear(1))
      return true;
    default:
      TC3_LOG(ERROR) << "Unknown relation type: "
                     << static_cast<int>(date_time_component.component_type);
      return false;
  }
  return false;
}

template <class TCalendar>
DatetimeGranularity CalendarLibTempl<TCalendar>::GetGranularity(
    const DatetimeParsedData& data) const {
  return data.GetFinestGranularity();
}

};  // namespace calendar

#undef TC3_CALENDAR_CHECK

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_COMMON_H_
