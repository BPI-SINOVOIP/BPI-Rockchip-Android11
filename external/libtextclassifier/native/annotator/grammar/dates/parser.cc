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

#include "annotator/grammar/dates/parser.h"

#include "annotator/grammar/dates/extractor.h"
#include "annotator/grammar/dates/utils/date-match.h"
#include "annotator/grammar/dates/utils/date-utils.h"
#include "utils/base/integral_types.h"
#include "utils/base/logging.h"
#include "utils/base/macros.h"
#include "utils/grammar/lexer.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/types.h"
#include "utils/strings/split.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3::dates {
namespace {

// Helper methods to validate individual components from a date match.

// Checks the validation requirement of a rule against a match.
// For example if the rule asks for `SPELLED_MONTH`, then we check that the
// match has the right flag.
bool CheckMatchValidationAndFlag(
    const grammar::Match* match, const ExtractionRuleParameter* rule,
    const ExtractionRuleParameter_::ExtractionValidation validation,
    const NonterminalParameter_::Flag flag) {
  if (rule == nullptr || (rule->validation() & validation) == 0) {
    // No validation requirement.
    return true;
  }
  const NonterminalParameter* nonterminal_parameter =
      static_cast<const NonterminalMatch*>(match)
          ->nonterminal->nonterminal_parameter();
  return (nonterminal_parameter != nullptr &&
          (nonterminal_parameter->flag() & flag) != 0);
}

bool GenerateDate(const ExtractionRuleParameter* rule,
                  const grammar::Match* match, DateMatch* date) {
  bool is_valid = true;

  // Post check and assign date components.
  grammar::Traverse(match, [rule, date, &is_valid](const grammar::Match* node) {
    switch (node->type) {
      case MatchType_YEAR: {
        if (CheckMatchValidationAndFlag(
                node, rule,
                ExtractionRuleParameter_::ExtractionValidation_SPELLED_YEAR,
                NonterminalParameter_::Flag_IS_SPELLED)) {
          date->year_match = static_cast<const YearMatch*>(node);
          date->year = date->year_match->value;
        } else {
          is_valid = false;
        }
        break;
      }
      case MatchType_MONTH: {
        if (CheckMatchValidationAndFlag(
                node, rule,
                ExtractionRuleParameter_::ExtractionValidation_SPELLED_MONTH,
                NonterminalParameter_::Flag_IS_SPELLED)) {
          date->month_match = static_cast<const MonthMatch*>(node);
          date->month = date->month_match->value;
        } else {
          is_valid = false;
        }
        break;
      }
      case MatchType_DAY: {
        if (CheckMatchValidationAndFlag(
                node, rule,
                ExtractionRuleParameter_::ExtractionValidation_SPELLED_DAY,
                NonterminalParameter_::Flag_IS_SPELLED)) {
          date->day_match = static_cast<const DayMatch*>(node);
          date->day = date->day_match->value;
        } else {
          is_valid = false;
        }
        break;
      }
      case MatchType_DAY_OF_WEEK: {
        date->day_of_week_match = static_cast<const DayOfWeekMatch*>(node);
        date->day_of_week =
            static_cast<DayOfWeek>(date->day_of_week_match->value);
        break;
      }
      case MatchType_TIME_VALUE: {
        date->time_value_match = static_cast<const TimeValueMatch*>(node);
        date->hour = date->time_value_match->hour;
        date->minute = date->time_value_match->minute;
        date->second = date->time_value_match->second;
        date->fraction_second = date->time_value_match->fraction_second;
        return false;
      }
      case MatchType_TIME_SPAN: {
        date->time_span_match = static_cast<const TimeSpanMatch*>(node);
        date->time_span_code = date->time_span_match->time_span_code;
        return false;
      }
      case MatchType_TIME_ZONE_NAME: {
        date->time_zone_name_match =
            static_cast<const TimeZoneNameMatch*>(node);
        date->time_zone_code = date->time_zone_name_match->time_zone_code;
        return false;
      }
      case MatchType_TIME_ZONE_OFFSET: {
        date->time_zone_offset_match =
            static_cast<const TimeZoneOffsetMatch*>(node);
        date->time_zone_offset = date->time_zone_offset_match->time_zone_offset;
        return false;
      }
      case MatchType_RELATIVE_DATE: {
        date->relative_match = static_cast<const RelativeMatch*>(node);
        return false;
      }
      case MatchType_COMBINED_DIGITS: {
        date->combined_digits_match =
            static_cast<const CombinedDigitsMatch*>(node);
        if (date->combined_digits_match->HasYear()) {
          date->year = date->combined_digits_match->GetYear();
        }
        if (date->combined_digits_match->HasMonth()) {
          date->month = date->combined_digits_match->GetMonth();
        }
        if (date->combined_digits_match->HasDay()) {
          date->day = date->combined_digits_match->GetDay();
        }
        if (date->combined_digits_match->HasHour()) {
          date->hour = date->combined_digits_match->GetHour();
        }
        if (date->combined_digits_match->HasMinute()) {
          date->minute = date->combined_digits_match->GetMinute();
        }
        if (date->combined_digits_match->HasSecond()) {
          date->second = date->combined_digits_match->GetSecond();
        }
        return false;
      }
      default:
        // Expand node further.
        return true;
    }

    return false;
  });

  if (is_valid) {
    date->begin = match->codepoint_span.first;
    date->end = match->codepoint_span.second;
    date->priority = rule ? rule->priority_delta() : 0;
    date->annotator_priority_score =
        rule ? rule->annotator_priority_score() : 0.0;
  }
  return is_valid;
}

bool GenerateFromOrToDateRange(const grammar::Match* match, DateMatch* date) {
  return GenerateDate(
      /*rule=*/(
          match->type == MatchType_DATETIME
              ? static_cast<const ExtractionMatch*>(match)->extraction_rule
              : nullptr),
      match, date);
}

bool GenerateDateRange(const grammar::Match* match, const grammar::Match* from,
                       const grammar::Match* to, DateRangeMatch* date_range) {
  if (!GenerateFromOrToDateRange(from, &date_range->from)) {
    TC3_LOG(WARNING) << "Failed to generate date for `from`.";
    return false;
  }
  if (!GenerateFromOrToDateRange(to, &date_range->to)) {
    TC3_LOG(WARNING) << "Failed to generate date for `to`.";
    return false;
  }
  date_range->begin = match->codepoint_span.first;
  date_range->end = match->codepoint_span.second;
  return true;
}

bool NormalizeHour(DateMatch* date) {
  if (date->time_span_match == nullptr) {
    // Nothing to do.
    return true;
  }
  return NormalizeHourByTimeSpan(date->time_span_match->time_span_spec, date);
}

void CheckAndSetAmbiguousHour(DateMatch* date) {
  if (date->HasHour()) {
    // Use am-pm ambiguity as default.
    if (!date->HasTimeSpanCode() && date->hour >= 1 && date->hour <= 12 &&
        !(date->time_value_match != nullptr &&
          date->time_value_match->hour_match != nullptr &&
          date->time_value_match->hour_match->is_zero_prefixed)) {
      date->SetAmbiguousHourProperties(2, 12);
    }
  }
}

// Normalizes a date candidate.
// Returns whether the candidate was successfully normalized.
bool NormalizeDate(DateMatch* date) {
  // Normalize hour.
  if (!NormalizeHour(date)) {
    TC3_VLOG(ERROR) << "Hour normalization (according to time-span) failed."
                    << date->DebugString();
    return false;
  }
  CheckAndSetAmbiguousHour(date);
  if (!date->IsValid()) {
    TC3_VLOG(ERROR) << "Fields inside date instance are ill-formed "
                    << date->DebugString();
  }
  return true;
}

// Copies the field from one DateMatch to another whose field is null. for
// example: if the from is "May 1, 8pm", and the to is "9pm", "May 1" will be
// copied to "to". Now we only copy fields for date range requirement.fv
void CopyFieldsForDateMatch(const DateMatch& from, DateMatch* to) {
  if (from.time_span_match != nullptr && to->time_span_match == nullptr) {
    to->time_span_match = from.time_span_match;
    to->time_span_code = from.time_span_code;
  }
  if (from.month_match != nullptr && to->month_match == nullptr) {
    to->month_match = from.month_match;
    to->month = from.month;
  }
}

// Normalizes a date range candidate.
// Returns whether the date range was successfully normalized.
bool NormalizeDateRange(DateRangeMatch* date_range) {
  CopyFieldsForDateMatch(date_range->from, &date_range->to);
  CopyFieldsForDateMatch(date_range->to, &date_range->from);
  return (NormalizeDate(&date_range->from) && NormalizeDate(&date_range->to));
}

bool CheckDate(const DateMatch& date, const ExtractionRuleParameter* rule) {
  // It's possible that "time_zone_name_match == NULL" when
  // "HasTimeZoneCode() == true", or "time_zone_offset_match == NULL" when
  // "HasTimeZoneOffset() == true" due to inference between endpoints, so we
  // must check if they really exist before using them.
  if (date.HasTimeZoneOffset()) {
    if (date.HasTimeZoneCode()) {
      if (date.time_zone_name_match != nullptr) {
        TC3_CHECK(date.time_zone_name_match->time_zone_name_spec != nullptr);
        const TimeZoneNameSpec* spec =
            date.time_zone_name_match->time_zone_name_spec;
        if (!spec->is_utc()) {
          return false;
        }
        if (!spec->is_abbreviation()) {
          return false;
        }
      }
    } else if (date.time_zone_offset_match != nullptr) {
      TC3_CHECK(date.time_zone_offset_match->time_zone_offset_param != nullptr);
      const TimeZoneOffsetParameter* param =
          date.time_zone_offset_match->time_zone_offset_param;
      if (param->format() == TimeZoneOffsetParameter_::Format_FORMAT_H ||
          param->format() == TimeZoneOffsetParameter_::Format_FORMAT_HH) {
        return false;
      }
      if (!(rule->validation() &
            ExtractionRuleParameter_::
                ExtractionValidation_ALLOW_UNCONFIDENT_TIME_ZONE)) {
        if (param->format() == TimeZoneOffsetParameter_::Format_FORMAT_H_MM ||
            param->format() == TimeZoneOffsetParameter_::Format_FORMAT_HH_MM ||
            param->format() == TimeZoneOffsetParameter_::Format_FORMAT_HMM) {
          return false;
        }
      }
    }
  }

  // Case: 1 April could be extracted as year 1, month april.
  // We simply remove this case.
  if (!date.HasBcAd() && date.year_match != nullptr && date.year < 1000) {
    // We allow case like 11/5/01
    if (date.HasMonth() && date.HasDay() &&
        date.year_match->count_of_digits == 2) {
    } else {
      return false;
    }
  }

  // Ignore the date if the year is larger than 9999 (The maximum number of 4
  // digits).
  if (date.year_match != nullptr && date.year > 9999) {
    TC3_VLOG(ERROR) << "Year is greater than 9999.";
    return false;
  }

  // Case: spelled may could be month 5, it also used very common as modal
  // verbs. We ignore spelled may as month.
  if ((rule->validation() &
       ExtractionRuleParameter_::ExtractionValidation_SPELLED_MONTH) &&
      date.month == 5 && !date.HasYear() && !date.HasDay()) {
    return false;
  }

  return true;
}

bool CheckContext(const std::vector<UnicodeText::const_iterator>& text,
                  const DateExtractor::Output& output) {
  const uint32 validation = output.rule->validation();

  // Nothing to check if we don't have any validation requirements for the
  // span boundaries.
  if ((validation &
       (ExtractionRuleParameter_::ExtractionValidation_LEFT_BOUND |
        ExtractionRuleParameter_::ExtractionValidation_RIGHT_BOUND)) == 0) {
    return true;
  }

  const int begin = output.match->codepoint_span.first;
  const int end = output.match->codepoint_span.second;

  // So far, we only check that the adjacent character cannot be a separator,
  // like /, - or .
  if ((validation &
       ExtractionRuleParameter_::ExtractionValidation_LEFT_BOUND) != 0) {
    if (begin > 0 && (*text[begin - 1] == '/' || *text[begin - 1] == '-' ||
                      *text[begin - 1] == ':')) {
      return false;
    }
  }
  if ((validation &
       ExtractionRuleParameter_::ExtractionValidation_RIGHT_BOUND) != 0) {
    // Last valid codepoint is at text.size() - 2 as we added the end position
    // of text for easier span extraction.
    if (end < text.size() - 1 &&
        (*text[end] == '/' || *text[end] == '-' || *text[end] == ':')) {
      return false;
    }
  }

  return true;
}

// Validates a date match. Returns true if the candidate is valid.
bool ValidateDate(const std::vector<UnicodeText::const_iterator>& text,
                  const DateExtractor::Output& output, const DateMatch& date) {
  if (!CheckDate(date, output.rule)) {
    return false;
  }
  if (!CheckContext(text, output)) {
    return false;
  }
  return true;
}

// Builds matched date instances from the grammar output.
std::vector<DateMatch> BuildDateMatches(
    const std::vector<UnicodeText::const_iterator>& text,
    const std::vector<DateExtractor::Output>& outputs) {
  std::vector<DateMatch> result;
  for (const DateExtractor::Output& output : outputs) {
    DateMatch date;
    if (GenerateDate(output.rule, output.match, &date)) {
      if (!NormalizeDate(&date)) {
        continue;
      }
      if (!ValidateDate(text, output, date)) {
        continue;
      }
      result.push_back(date);
    }
  }
  return result;
}

// Builds matched date range instances from the grammar output.
std::vector<DateRangeMatch> BuildDateRangeMatches(
    const std::vector<UnicodeText::const_iterator>& text,
    const std::vector<DateExtractor::RangeOutput>& range_outputs) {
  std::vector<DateRangeMatch> result;
  for (const DateExtractor::RangeOutput& range_output : range_outputs) {
    DateRangeMatch date_range;
    if (GenerateDateRange(range_output.match, range_output.from,
                          range_output.to, &date_range)) {
      if (!NormalizeDateRange(&date_range)) {
        continue;
      }
      result.push_back(date_range);
    }
  }
  return result;
}

template <typename T>
void RemoveDeletedMatches(const std::vector<bool>& removed,
                          std::vector<T>* matches) {
  int input = 0;
  for (int next = 0; next < matches->size(); ++next) {
    if (removed[next]) {
      continue;
    }
    if (input != next) {
      (*matches)[input] = (*matches)[next];
    }
    input++;
  }
  matches->resize(input);
}

// Removes duplicated date or date range instances.
// Overlapping date and date ranges are not considered here.
template <typename T>
void RemoveDuplicatedDates(std::vector<T>* matches) {
  // Assumption: matches are sorted ascending by (begin, end).
  std::vector<bool> removed(matches->size(), false);
  for (int i = 0; i < matches->size(); i++) {
    if (removed[i]) {
      continue;
    }
    const T& candidate = matches->at(i);
    for (int j = i + 1; j < matches->size(); j++) {
      if (removed[j]) {
        continue;
      }
      const T& next = matches->at(j);

      // Not overlapping.
      if (next.begin >= candidate.end) {
        break;
      }

      // If matching the same span of text, then check the priority.
      if (candidate.begin == next.begin && candidate.end == next.end) {
        if (candidate.GetPriority() < next.GetPriority()) {
          removed[i] = true;
          break;
        } else {
          removed[j] = true;
          continue;
        }
      }

      // Checks if `next` is fully covered by fields of `candidate`.
      if (next.end <= candidate.end) {
        removed[j] = true;
        continue;
      }

      // Checks whether `candidate`/`next` is a refinement.
      if (IsRefinement(candidate, next)) {
        removed[j] = true;
        continue;
      } else if (IsRefinement(next, candidate)) {
        removed[i] = true;
        break;
      }
    }
  }
  RemoveDeletedMatches(removed, matches);
}

// Filters out simple overtriggering simple matches.
bool IsBlacklistedDate(const UniLib& unilib,
                       const std::vector<UnicodeText::const_iterator>& text,
                       const DateMatch& match) {
  const int begin = match.begin;
  const int end = match.end;
  if (end - begin != 3) {
    return false;
  }

  std::string text_lower =
      unilib
          .ToLowerText(
              UTF8ToUnicodeText(text[begin].utf8_data(),
                                text[end].utf8_data() - text[begin].utf8_data(),
                                /*do_copy=*/false))
          .ToUTF8String();

  // "sun" is not a good abbreviation for a standalone day of the week.
  if (match.IsStandaloneRelativeDayOfWeek() &&
      (text_lower == "sun" || text_lower == "mon")) {
    return true;
  }

  // "mar" is not a good abbreviation for single month.
  if (match.HasMonth() && text_lower == "mar") {
    return true;
  }

  return false;
}

// Checks if two date matches are adjacent and mergeable.
bool AreDateMatchesAdjacentAndMergeable(
    const UniLib& unilib, const std::vector<UnicodeText::const_iterator>& text,
    const std::vector<std::string>& ignored_spans, const DateMatch& prev,
    const DateMatch& next) {
  // Check the context between the two matches.
  if (next.begin <= prev.end) {
    // The two matches are not adjacent.
    return false;
  }
  UnicodeText span;
  for (int i = prev.end; i < next.begin; i++) {
    const char32 codepoint = *text[i];
    if (unilib.IsWhitespace(codepoint)) {
      continue;
    }
    span.push_back(unilib.ToLower(codepoint));
  }
  if (span.empty()) {
    return true;
  }
  const std::string span_text = span.ToUTF8String();
  bool matched = false;
  for (const std::string& ignored_span : ignored_spans) {
    if (span_text == ignored_span) {
      matched = true;
      break;
    }
  }
  if (!matched) {
    return false;
  }
  return IsDateMatchMergeable(prev, next);
}

// Merges adjacent date and date range.
// For e.g. Monday, 5-10pm, the date "Monday" and the time range "5-10pm" will
// be merged
void MergeDateRangeAndDate(const UniLib& unilib,
                           const std::vector<UnicodeText::const_iterator>& text,
                           const std::vector<std::string>& ignored_spans,
                           const std::vector<DateMatch>& dates,
                           std::vector<DateRangeMatch>* date_ranges) {
  // For each range, check the date before or after the it to see if they could
  // be merged. Both the range and date array are sorted, so we only need to
  // scan the date array once.
  int next_date = 0;
  for (int i = 0; i < date_ranges->size(); i++) {
    DateRangeMatch* date_range = &date_ranges->at(i);
    // So far we only merge time range with a date.
    if (!date_range->from.HasHour()) {
      continue;
    }

    for (; next_date < dates.size(); next_date++) {
      const DateMatch& date = dates[next_date];

      // If the range is before the date, we check whether `date_range->to` can
      // be merged with the date.
      if (date_range->end <= date.begin) {
        DateMatch merged_date = date;
        if (AreDateMatchesAdjacentAndMergeable(unilib, text, ignored_spans,
                                               date_range->to, date)) {
          MergeDateMatch(date_range->to, &merged_date, /*update_span=*/true);
          date_range->to = merged_date;
          date_range->end = date_range->to.end;
          MergeDateMatch(date, &date_range->from, /*update_span=*/false);
          next_date++;

          // Check the second date after the range to see if it could be merged
          // further. For example: 10-11pm, Monday, May 15. 10-11pm is merged
          // with Monday and then we check that it could be merged with May 15
          // as well.
          if (next_date < dates.size()) {
            DateMatch next_match = dates[next_date];
            if (AreDateMatchesAdjacentAndMergeable(
                    unilib, text, ignored_spans, date_range->to, next_match)) {
              MergeDateMatch(date_range->to, &next_match, /*update_span=*/true);
              date_range->to = next_match;
              date_range->end = date_range->to.end;
              MergeDateMatch(dates[next_date], &date_range->from,
                             /*update_span=*/false);
              next_date++;
            }
          }
        }
        // Since the range is before the date, we try to check if the next range
        // could be merged with the current date.
        break;
      } else if (date_range->end > date.end && date_range->begin > date.begin) {
        // If the range is after the date, we check if `date_range.from` can be
        // merged with the date. Here is a special case, the date before range
        // could be partially overlapped. This is because the range.from could
        // be extracted as year in date. For example: March 3, 10-11pm is
        // extracted as date March 3, 2010 and the range 10-11pm. In this
        // case, we simply clear the year from date.
        DateMatch merged_date = date;
        if (date.HasYear() &&
            date.year_match->codepoint_span.second > date_range->begin) {
          merged_date.year_match = nullptr;
          merged_date.year = NO_VAL;
          merged_date.end = date.year_match->match_offset;
        }
        // Check and merge the range and the date before the range.
        if (AreDateMatchesAdjacentAndMergeable(unilib, text, ignored_spans,
                                               merged_date, date_range->from)) {
          MergeDateMatch(merged_date, &date_range->from, /*update_span=*/true);
          date_range->begin = date_range->from.begin;
          MergeDateMatch(merged_date, &date_range->to, /*update_span=*/false);

          // Check if the second date before the range can be merged as well.
          if (next_date > 0) {
            DateMatch prev_match = dates[next_date - 1];
            if (prev_match.end <= date_range->from.begin) {
              if (AreDateMatchesAdjacentAndMergeable(unilib, text,
                                                     ignored_spans, prev_match,
                                                     date_range->from)) {
                MergeDateMatch(prev_match, &date_range->from,
                               /*update_span=*/true);
                date_range->begin = date_range->from.begin;
                MergeDateMatch(prev_match, &date_range->to,
                               /*update_span=*/false);
              }
            }
          }
          next_date++;
          break;
        } else {
          // Since the date is before the date range, we move to the next date
          // to check if it could be merged with the current range.
          continue;
        }
      } else {
        // The date is either fully overlapped by the date range or the date
        // span end is after the date range. Move to the next date in both
        // cases.
      }
    }
  }
}

// Removes the dates which are part of a range. e.g. in "May 1 - 3", the date
// "May 1" is fully contained in the range.
void RemoveOverlappedDateByRange(const std::vector<DateRangeMatch>& ranges,
                                 std::vector<DateMatch>* dates) {
  int next_date = 0;
  std::vector<bool> removed(dates->size(), false);
  for (int i = 0; i < ranges.size(); ++i) {
    const auto& range = ranges[i];
    for (; next_date < dates->size(); ++next_date) {
      const auto& date = dates->at(next_date);
      // So far we don't touch the partially overlapped case.
      if (date.begin >= range.begin && date.end <= range.end) {
        // Fully contained.
        removed[next_date] = true;
      } else if (date.end <= range.begin) {
        continue;  // date is behind range, go to next date
      } else if (date.begin >= range.end) {
        break;  // range is behind date, go to next range
      }
    }
  }
  RemoveDeletedMatches(removed, dates);
}

// Converts candidate dates and date ranges.
void FillDateInstances(
    const UniLib& unilib, const std::vector<UnicodeText::const_iterator>& text,
    const DateAnnotationOptions& options, std::vector<DateMatch>* date_matches,
    std::vector<DatetimeParseResultSpan>* datetime_parse_result_spans) {
  int i = 0;
  for (int j = 1; j < date_matches->size(); j++) {
    if (options.merge_adjacent_components &&
        AreDateMatchesAdjacentAndMergeable(unilib, text, options.ignored_spans,
                                           date_matches->at(i),
                                           date_matches->at(j))) {
      MergeDateMatch(date_matches->at(i), &date_matches->at(j), true);
    } else {
      if (!IsBlacklistedDate(unilib, text, date_matches->at(i))) {
        DatetimeParseResultSpan datetime_parse_result_span;
        FillDateInstance(date_matches->at(i), &datetime_parse_result_span);
        datetime_parse_result_spans->push_back(datetime_parse_result_span);
      }
    }
    i = j;
  }
  if (!IsBlacklistedDate(unilib, text, date_matches->at(i))) {
    DatetimeParseResultSpan datetime_parse_result_span;
    FillDateInstance(date_matches->at(i), &datetime_parse_result_span);
    datetime_parse_result_spans->push_back(datetime_parse_result_span);
  }
}

void FillDateRangeInstances(
    const std::vector<DateRangeMatch>& date_range_matches,
    std::vector<DatetimeParseResultSpan>* datetime_parse_result_spans) {
  for (const DateRangeMatch& date_range_match : date_range_matches) {
    DatetimeParseResultSpan datetime_parse_result_span;
    FillDateRangeInstance(date_range_match, &datetime_parse_result_span);
    datetime_parse_result_spans->push_back(datetime_parse_result_span);
  }
}

// Fills `DatetimeParseResultSpan`  from `DateMatch` and `DateRangeMatch`
// instances.
std::vector<DatetimeParseResultSpan> GetOutputAsAnnotationList(
    const UniLib& unilib, const DateExtractor& extractor,
    const std::vector<UnicodeText::const_iterator>& text,
    const DateAnnotationOptions& options) {
  std::vector<DatetimeParseResultSpan> datetime_parse_result_spans;
  std::vector<DateMatch> date_matches =
      BuildDateMatches(text, extractor.output());

  std::sort(
      date_matches.begin(), date_matches.end(),
      // Order by increasing begin, and decreasing end (decreasing length).
      [](const DateMatch& a, const DateMatch& b) {
        return (a.begin < b.begin || (a.begin == b.begin && a.end > b.end));
      });

  if (!date_matches.empty()) {
    RemoveDuplicatedDates(&date_matches);
  }

  if (options.enable_date_range) {
    std::vector<DateRangeMatch> date_range_matches =
        BuildDateRangeMatches(text, extractor.range_output());

    if (!date_range_matches.empty()) {
      std::sort(
          date_range_matches.begin(), date_range_matches.end(),
          // Order by increasing begin, and decreasing end (decreasing length).
          [](const DateRangeMatch& a, const DateRangeMatch& b) {
            return (a.begin < b.begin || (a.begin == b.begin && a.end > b.end));
          });
      RemoveDuplicatedDates(&date_range_matches);
    }

    if (!date_matches.empty()) {
      MergeDateRangeAndDate(unilib, text, options.ignored_spans, date_matches,
                            &date_range_matches);
      RemoveOverlappedDateByRange(date_range_matches, &date_matches);
    }
    FillDateRangeInstances(date_range_matches, &datetime_parse_result_spans);
  }

  if (!date_matches.empty()) {
    FillDateInstances(unilib, text, options, &date_matches,
                      &datetime_parse_result_spans);
  }
  return datetime_parse_result_spans;
}

}  // namespace

std::vector<DatetimeParseResultSpan> DateParser::Parse(
    StringPiece text, const std::vector<Token>& tokens,
    const std::vector<Locale>& locales,
    const DateAnnotationOptions& options) const {
  std::vector<UnicodeText::const_iterator> codepoint_offsets;
  const UnicodeText text_unicode = UTF8ToUnicodeText(text,
                                                     /*do_copy=*/false);
  for (auto it = text_unicode.begin(); it != text_unicode.end(); it++) {
    codepoint_offsets.push_back(it);
  }
  codepoint_offsets.push_back(text_unicode.end());
  DateExtractor extractor(codepoint_offsets, options, datetime_rules_);
  // Select locale matching rules.
  // Only use a shard if locales match or the shard doesn't specify a locale
  // restriction.
  std::vector<const grammar::RulesSet_::Rules*> locale_rules =
      SelectLocaleMatchingShards(datetime_rules_->rules(), rules_locales_,
                                 locales);
  if (locale_rules.empty()) {
    return {};
  }
  grammar::Matcher matcher(&unilib_, datetime_rules_->rules(), locale_rules,
                           &extractor);
  lexer_.Process(text_unicode, tokens, /*annotations=*/nullptr, &matcher);
  return GetOutputAsAnnotationList(unilib_, extractor, codepoint_offsets,
                                   options);
}

}  // namespace libtextclassifier3::dates
