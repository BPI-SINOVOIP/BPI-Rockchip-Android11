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

#include "annotator/grammar/dates/extractor.h"

#include <initializer_list>
#include <map>

#include "annotator/grammar/dates/utils/date-match.h"
#include "annotator/grammar/dates/utils/date-utils.h"
#include "utils/base/casts.h"
#include "utils/base/logging.h"
#include "utils/strings/numbers.h"

namespace libtextclassifier3::dates {
namespace {

// Helper struct for time-related components.
// Extracts all subnodes of a specified type.
struct MatchComponents {
  MatchComponents(const grammar::Match* root,
                  std::initializer_list<int16> types)
      : root(root),
        components(grammar::SelectAll(
            root, [root, &types](const grammar::Match* node) {
              if (node == root || node->type == grammar::Match::kUnknownType) {
                return false;
              }
              for (const int64 type : types) {
                if (node->type == type) {
                  return true;
                }
              }
              return false;
            })) {}

  // Returns the index of the first submatch of the specified type or -1 if not
  // found.
  int IndexOf(const int16 type, const int start_index = 0) const {
    for (int i = start_index; i < components.size(); i++) {
      if (components[i]->type == type) {
        return i;
      }
    }
    return -1;
  }

  // Returns the first submatch of the specified type, or nullptr if not found.
  template <typename T>
  const T* SubmatchOf(const int16 type, const int start_index = 0) const {
    return SubmatchAt<T>(IndexOf(type, start_index));
  }

  template <typename T>
  const T* SubmatchAt(const int index) const {
    if (index < 0) {
      return nullptr;
    }
    return static_cast<const T*>(components[index]);
  }

  const grammar::Match* root;
  std::vector<const grammar::Match*> components;
};

// Helper method to check whether a time value has valid components.
bool IsValidTimeValue(const TimeValueMatch& time_value) {
  // Can only specify seconds if minutes are present.
  if (time_value.minute == NO_VAL && time_value.second != NO_VAL) {
    return false;
  }
  // Can only specify fraction of seconds if seconds are present.
  if (time_value.second == NO_VAL && time_value.fraction_second >= 0.0) {
    return false;
  }

  const int8 h = time_value.hour;
  const int8 m = (time_value.minute < 0 ? 0 : time_value.minute);
  const int8 s = (time_value.second < 0 ? 0 : time_value.second);
  const double f =
      (time_value.fraction_second < 0.0 ? 0.0 : time_value.fraction_second);

  // Check value bounds.
  if (h == NO_VAL || h > 24 || m > 59 || s > 60) {
    return false;
  }
  if (h == 24 && (m != 0 || s != 0 || f > 0.0)) {
    return false;
  }
  if (s == 60 && m != 59) {
    return false;
  }
  return true;
}

int ParseLeadingDec32Value(const char* c_str) {
  int value;
  if (ParseInt32(c_str, &value)) {
    return value;
  }
  return NO_VAL;
}

double ParseLeadingDoubleValue(const char* c_str) {
  double value;
  if (ParseDouble(c_str, &value)) {
    return value;
  }
  return NO_VAL;
}

// Extracts digits as an integer and adds a typed match accordingly.
template <typename T>
void CheckDigits(const grammar::Match* match,
                 const NonterminalValue* nonterminal, StringPiece match_text,
                 grammar::Matcher* matcher) {
  TC3_CHECK(match->IsUnaryRule());
  const int value = ParseLeadingDec32Value(match_text.ToString().c_str());
  if (!T::IsValid(value)) {
    return;
  }
  const int num_digits = match_text.size();
  T* result = matcher->AllocateAndInitMatch<T>(
      match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  result->value = value;
  result->count_of_digits = num_digits;
  result->is_zero_prefixed = (num_digits >= 2 && match_text[0] == '0');
  matcher->AddMatch(result);
}

// Extracts digits as a decimal (as fraction, as if a "0." is prefixed) and
// adds a typed match to the `er accordingly.
template <typename T>
void CheckDigitsAsFraction(const grammar::Match* match,
                           const NonterminalValue* nonterminal,
                           StringPiece match_text, grammar::Matcher* matcher) {
  TC3_CHECK(match->IsUnaryRule());
  // TODO(smillius): Should should be achievable in a more straight-forward way.
  const double value =
      ParseLeadingDoubleValue(("0." + match_text.ToString()).data());
  if (!T::IsValid(value)) {
    return;
  }
  T* result = matcher->AllocateAndInitMatch<T>(
      match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  result->value = value;
  result->count_of_digits = match_text.size();
  matcher->AddMatch(result);
}

// Extracts consecutive digits as multiple integers according to a format and
// adds a type match to the matcher accordingly.
template <typename T>
void CheckCombinedDigits(const grammar::Match* match,
                         const NonterminalValue* nonterminal,
                         StringPiece match_text, grammar::Matcher* matcher) {
  TC3_CHECK(match->IsUnaryRule());
  const std::string& format =
      nonterminal->nonterminal_parameter()->combined_digits_format()->str();
  if (match_text.size() != format.size()) {
    return;
  }

  static std::map<char, CombinedDigitsMatch::Index>& kCombinedDigitsMatchIndex =
      *[]() {
        return new std::map<char, CombinedDigitsMatch::Index>{
            {'Y', CombinedDigitsMatch::INDEX_YEAR},
            {'M', CombinedDigitsMatch::INDEX_MONTH},
            {'D', CombinedDigitsMatch::INDEX_DAY},
            {'h', CombinedDigitsMatch::INDEX_HOUR},
            {'m', CombinedDigitsMatch::INDEX_MINUTE},
            {'s', CombinedDigitsMatch::INDEX_SECOND}};
      }();

  struct Segment {
    const int index;
    const int length;
    const int value;
  };
  std::vector<Segment> segments;
  int slice_start = 0;
  while (slice_start < format.size()) {
    int slice_end = slice_start + 1;
    // Advace right as long as we have the same format character.
    while (slice_end < format.size() &&
           format[slice_start] == format[slice_end]) {
      slice_end++;
    }

    const int slice_length = slice_end - slice_start;
    const int value = ParseLeadingDec32Value(
        std::string(match_text.data() + slice_start, slice_length).c_str());

    auto index = kCombinedDigitsMatchIndex.find(format[slice_start]);
    if (index == kCombinedDigitsMatchIndex.end()) {
      return;
    }
    if (!T::IsValid(index->second, value)) {
      return;
    }
    segments.push_back(Segment{index->second, slice_length, value});
    slice_start = slice_end;
  }
  T* result = matcher->AllocateAndInitMatch<T>(
      match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  for (const Segment& segment : segments) {
    result->values[segment.index] = segment.value;
  }
  result->count_of_digits = match_text.size();
  result->is_zero_prefixed =
      (match_text[0] == '0' && segments.front().length >= 2);
  matcher->AddMatch(result);
}

// Retrieves the corresponding value from an associated term-value mapping for
// the nonterminal and adds a typed match to the matcher accordingly.
template <typename T>
void CheckMappedValue(const grammar::Match* match,
                      const NonterminalValue* nonterminal,
                      grammar::Matcher* matcher) {
  const TermValueMatch* term =
      grammar::SelectFirstOfType<TermValueMatch>(match, MatchType_TERM_VALUE);
  if (term == nullptr) {
    return;
  }
  const int value = term->term_value->value();
  if (!T::IsValid(value)) {
    return;
  }
  T* result = matcher->AllocateAndInitMatch<T>(
      match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  result->value = value;
  matcher->AddMatch(result);
}

// Checks if there is an associated value in the corresponding nonterminal and
// adds a typed match to the matcher accordingly.
template <typename T>
void CheckDirectValue(const grammar::Match* match,
                      const NonterminalValue* nonterminal,
                      grammar::Matcher* matcher) {
  const int value = nonterminal->value()->value();
  if (!T::IsValid(value)) {
    return;
  }
  T* result = matcher->AllocateAndInitMatch<T>(
      match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  result->value = value;
  matcher->AddMatch(result);
}

template <typename T>
void CheckAndAddDirectOrMappedValue(const grammar::Match* match,
                                    const NonterminalValue* nonterminal,
                                    grammar::Matcher* matcher) {
  if (nonterminal->value() != nullptr) {
    CheckDirectValue<T>(match, nonterminal, matcher);
  } else {
    CheckMappedValue<T>(match, nonterminal, matcher);
  }
}

template <typename T>
void CheckAndAddNumericValue(const grammar::Match* match,
                             const NonterminalValue* nonterminal,
                             StringPiece match_text,
                             grammar::Matcher* matcher) {
  if (nonterminal->nonterminal_parameter() != nullptr &&
      nonterminal->nonterminal_parameter()->flag() &
          NonterminalParameter_::Flag_IS_SPELLED) {
    CheckMappedValue<T>(match, nonterminal, matcher);
  } else {
    CheckDigits<T>(match, nonterminal, match_text, matcher);
  }
}

// Tries to parse as digital time value.
bool ParseDigitalTimeValue(const std::vector<UnicodeText::const_iterator>& text,
                           const MatchComponents& components,
                           const NonterminalValue* nonterminal,
                           grammar::Matcher* matcher) {
  // Required fields.
  const HourMatch* hour = components.SubmatchOf<HourMatch>(MatchType_HOUR);
  if (hour == nullptr || hour->count_of_digits == 0) {
    return false;
  }

  // Optional fields.
  const MinuteMatch* minute =
      components.SubmatchOf<MinuteMatch>(MatchType_MINUTE);
  if (minute != nullptr && minute->count_of_digits == 0) {
    return false;
  }
  const SecondMatch* second =
      components.SubmatchOf<SecondMatch>(MatchType_SECOND);
  if (second != nullptr && second->count_of_digits == 0) {
    return false;
  }
  const FractionSecondMatch* fraction_second =
      components.SubmatchOf<FractionSecondMatch>(MatchType_FRACTION_SECOND);
  if (fraction_second != nullptr && fraction_second->count_of_digits == 0) {
    return false;
  }

  // Validation.
  uint32 validation = nonterminal->time_value_parameter()->validation();
  const grammar::Match* end = hour;
  if (minute != nullptr) {
    if (second != nullptr) {
      if (fraction_second != nullptr) {
        end = fraction_second;
      } else {
        end = second;
      }
    } else {
      end = minute;
    }
  }

  // Check if there is any extra space between h m s f.
  if ((validation &
       TimeValueParameter_::TimeValueValidation_ALLOW_EXTRA_SPACE) == 0) {
    // Check whether there is whitespace between token.
    if (minute != nullptr && minute->HasLeadingWhitespace()) {
      return false;
    }
    if (second != nullptr && second->HasLeadingWhitespace()) {
      return false;
    }
    if (fraction_second != nullptr && fraction_second->HasLeadingWhitespace()) {
      return false;
    }
  }

  // Check if there is any ':' or '.' as a prefix or suffix.
  if (validation &
      TimeValueParameter_::TimeValueValidation_DISALLOW_COLON_DOT_CONTEXT) {
    const int begin_pos = hour->codepoint_span.first;
    const int end_pos = end->codepoint_span.second;
    if (begin_pos > 1 &&
        (*text[begin_pos - 1] == ':' || *text[begin_pos - 1] == '.') &&
        isdigit(*text[begin_pos - 2])) {
      return false;
    }
    // Last valid codepoint is at text.size() - 2 as we added the end position
    // of text for easier span extraction.
    if (end_pos < text.size() - 2 &&
        (*text[end_pos] == ':' || *text[end_pos] == '.') &&
        isdigit(*text[end_pos + 1])) {
      return false;
    }
  }

  TimeValueMatch time_value;
  time_value.Init(components.root->lhs, components.root->codepoint_span,
                  components.root->match_offset);
  time_value.Reset();
  time_value.hour_match = hour;
  time_value.minute_match = minute;
  time_value.second_match = second;
  time_value.fraction_second_match = fraction_second;
  time_value.is_hour_zero_prefixed = hour->is_zero_prefixed;
  time_value.is_minute_one_digit =
      (minute != nullptr && minute->count_of_digits == 1);
  time_value.is_second_one_digit =
      (second != nullptr && second->count_of_digits == 1);
  time_value.hour = hour->value;
  time_value.minute = (minute != nullptr ? minute->value : NO_VAL);
  time_value.second = (second != nullptr ? second->value : NO_VAL);
  time_value.fraction_second =
      (fraction_second != nullptr ? fraction_second->value : NO_VAL);

  if (!IsValidTimeValue(time_value)) {
    return false;
  }

  TimeValueMatch* result = matcher->AllocateMatch<TimeValueMatch>();
  *result = time_value;
  matcher->AddMatch(result);
  return true;
}

// Tries to parsing a time from spelled out time components.
bool ParseSpelledTimeValue(const MatchComponents& components,
                           const NonterminalValue* nonterminal,
                           grammar::Matcher* matcher) {
  // Required fields.
  const HourMatch* hour = components.SubmatchOf<HourMatch>(MatchType_HOUR);
  if (hour == nullptr || hour->count_of_digits != 0) {
    return false;
  }
  // Optional fields.
  const MinuteMatch* minute =
      components.SubmatchOf<MinuteMatch>(MatchType_MINUTE);
  if (minute != nullptr && minute->count_of_digits != 0) {
    return false;
  }
  const SecondMatch* second =
      components.SubmatchOf<SecondMatch>(MatchType_SECOND);
  if (second != nullptr && second->count_of_digits != 0) {
    return false;
  }

  uint32 validation = nonterminal->time_value_parameter()->validation();
  // Check if there is any extra space between h m s.
  if ((validation &
       TimeValueParameter_::TimeValueValidation_ALLOW_EXTRA_SPACE) == 0) {
    // Check whether there is whitespace between token.
    if (minute != nullptr && minute->HasLeadingWhitespace()) {
      return false;
    }
    if (second != nullptr && second->HasLeadingWhitespace()) {
      return false;
    }
  }

  TimeValueMatch time_value;
  time_value.Init(components.root->lhs, components.root->codepoint_span,
                  components.root->match_offset);
  time_value.Reset();
  time_value.hour_match = hour;
  time_value.minute_match = minute;
  time_value.second_match = second;
  time_value.is_hour_zero_prefixed = hour->is_zero_prefixed;
  time_value.is_minute_one_digit =
      (minute != nullptr && minute->count_of_digits == 1);
  time_value.is_second_one_digit =
      (second != nullptr && second->count_of_digits == 1);
  time_value.hour = hour->value;
  time_value.minute = (minute != nullptr ? minute->value : NO_VAL);
  time_value.second = (second != nullptr ? second->value : NO_VAL);

  if (!IsValidTimeValue(time_value)) {
    return false;
  }

  TimeValueMatch* result = matcher->AllocateMatch<TimeValueMatch>();
  *result = time_value;
  matcher->AddMatch(result);
  return true;
}

// Reconstructs and validates a time value from a match.
void CheckTimeValue(const std::vector<UnicodeText::const_iterator>& text,
                    const grammar::Match* match,
                    const NonterminalValue* nonterminal,
                    grammar::Matcher* matcher) {
  MatchComponents components(
      match, {MatchType_HOUR, MatchType_MINUTE, MatchType_SECOND,
              MatchType_FRACTION_SECOND});
  if (ParseDigitalTimeValue(text, components, nonterminal, matcher)) {
    return;
  }
  if (ParseSpelledTimeValue(components, nonterminal, matcher)) {
    return;
  }
}

// Validates a time span match.
void CheckTimeSpan(const grammar::Match* match,
                   const NonterminalValue* nonterminal,
                   grammar::Matcher* matcher) {
  const TermValueMatch* ts_name =
      grammar::SelectFirstOfType<TermValueMatch>(match, MatchType_TERM_VALUE);
  const TermValue* term_value = ts_name->term_value;
  TC3_CHECK(term_value != nullptr);
  TC3_CHECK(term_value->time_span_spec() != nullptr);
  const TimeSpanSpec* ts_spec = term_value->time_span_spec();
  TimeSpanMatch* time_span = matcher->AllocateAndInitMatch<TimeSpanMatch>(
      match->lhs, match->codepoint_span, match->match_offset);
  time_span->Reset();
  time_span->nonterminal = nonterminal;
  time_span->time_span_spec = ts_spec;
  time_span->time_span_code = ts_spec->code();
  matcher->AddMatch(time_span);
}

// Validates a time period match.
void CheckTimePeriod(const std::vector<UnicodeText::const_iterator>& text,
                     const grammar::Match* match,
                     const NonterminalValue* nonterminal,
                     grammar::Matcher* matcher) {
  int period_value = NO_VAL;

  // If a value mapping exists, use it.
  if (nonterminal->value() != nullptr) {
    period_value = nonterminal->value()->value();
  } else if (const TermValueMatch* term =
                 grammar::SelectFirstOfType<TermValueMatch>(
                     match, MatchType_TERM_VALUE)) {
    period_value = term->term_value->value();
  } else if (const grammar::Match* digits =
                 grammar::SelectFirstOfType<grammar::Match>(
                     match, grammar::Match::kDigitsType)) {
    period_value = ParseLeadingDec32Value(
        std::string(text[digits->codepoint_span.first].utf8_data(),
                    text[digits->codepoint_span.second].utf8_data() -
                        text[digits->codepoint_span.first].utf8_data())
            .c_str());
  }

  if (period_value <= NO_VAL) {
    return;
  }

  TimePeriodMatch* result = matcher->AllocateAndInitMatch<TimePeriodMatch>(
      match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  result->value = period_value;
  matcher->AddMatch(result);
}

// Reconstructs a date from a relative date rule match.
void CheckRelativeDate(const DateAnnotationOptions& options,
                       const grammar::Match* match,
                       const NonterminalValue* nonterminal,
                       grammar::Matcher* matcher) {
  if (!options.enable_special_day_offset &&
      grammar::SelectFirstOfType<TermValueMatch>(match, MatchType_TERM_VALUE) !=
          nullptr) {
    // Special day offsets, like "Today", "Tomorrow" etc. are not enabled.
    return;
  }

  RelativeMatch* relative_match = matcher->AllocateAndInitMatch<RelativeMatch>(
      match->lhs, match->codepoint_span, match->match_offset);
  relative_match->Reset();
  relative_match->nonterminal = nonterminal;

  // Fill relative date information from individual components.
  grammar::Traverse(match, [match, relative_match](const grammar::Match* node) {
    // Ignore the current match.
    if (node == match || node->type == grammar::Match::kUnknownType) {
      return true;
    }

    if (node->type == MatchType_TERM_VALUE) {
      const int value =
          static_cast<const TermValueMatch*>(node)->term_value->value();
      relative_match->day = abs(value);
      if (value >= 0) {
        // Marks "today" as in the future.
        relative_match->is_future_date = true;
      }
      relative_match->existing |=
          (RelativeMatch::HAS_DAY | RelativeMatch::HAS_IS_FUTURE);
      return false;
    }

    // Parse info from nonterminal.
    const NonterminalValue* nonterminal =
        static_cast<const NonterminalMatch*>(node)->nonterminal;
    if (nonterminal != nullptr &&
        nonterminal->relative_parameter() != nullptr) {
      const RelativeParameter* relative_parameter =
          nonterminal->relative_parameter();
      if (relative_parameter->period() !=
          RelativeParameter_::Period_PERIOD_UNKNOWN) {
        relative_match->is_future_date =
            (relative_parameter->period() ==
             RelativeParameter_::Period_PERIOD_FUTURE);
        relative_match->existing |= RelativeMatch::HAS_IS_FUTURE;
      }
      if (relative_parameter->day_of_week_interpretation() != nullptr) {
        relative_match->day_of_week_nonterminal = nonterminal;
        relative_match->existing |= RelativeMatch::HAS_DAY_OF_WEEK;
      }
    }

    // Relative day of week.
    if (node->type == MatchType_DAY_OF_WEEK) {
      relative_match->day_of_week =
          static_cast<const DayOfWeekMatch*>(node)->value;
      return false;
    }

    if (node->type != MatchType_TIME_PERIOD) {
      return true;
    }

    const TimePeriodMatch* period = static_cast<const TimePeriodMatch*>(node);
    switch (nonterminal->relative_parameter()->type()) {
      case RelativeParameter_::RelativeType_YEAR: {
        relative_match->year = period->value;
        relative_match->existing |= RelativeMatch::HAS_YEAR;
        break;
      }
      case RelativeParameter_::RelativeType_MONTH: {
        relative_match->month = period->value;
        relative_match->existing |= RelativeMatch::HAS_MONTH;
        break;
      }
      case RelativeParameter_::RelativeType_WEEK: {
        relative_match->week = period->value;
        relative_match->existing |= RelativeMatch::HAS_WEEK;
        break;
      }
      case RelativeParameter_::RelativeType_DAY: {
        relative_match->day = period->value;
        relative_match->existing |= RelativeMatch::HAS_DAY;
        break;
      }
      case RelativeParameter_::RelativeType_HOUR: {
        relative_match->hour = period->value;
        relative_match->existing |= RelativeMatch::HAS_HOUR;
        break;
      }
      case RelativeParameter_::RelativeType_MINUTE: {
        relative_match->minute = period->value;
        relative_match->existing |= RelativeMatch::HAS_MINUTE;
        break;
      }
      case RelativeParameter_::RelativeType_SECOND: {
        relative_match->second = period->value;
        relative_match->existing |= RelativeMatch::HAS_SECOND;
        break;
      }
      default:
        break;
    }

    return true;
  });
  matcher->AddMatch(relative_match);
}

bool IsValidTimeZoneOffset(const int time_zone_offset) {
  return (time_zone_offset >= -720 && time_zone_offset <= 840 &&
          time_zone_offset % 15 == 0);
}

// Parses, validates and adds a time zone offset match.
void CheckTimeZoneOffset(const grammar::Match* match,
                         const NonterminalValue* nonterminal,
                         grammar::Matcher* matcher) {
  MatchComponents components(
      match, {MatchType_DIGITS, MatchType_TERM_VALUE, MatchType_NONTERMINAL});
  const TermValueMatch* tz_sign =
      components.SubmatchOf<TermValueMatch>(MatchType_TERM_VALUE);
  if (tz_sign == nullptr) {
    return;
  }
  const int sign = tz_sign->term_value->value();
  TC3_CHECK(sign == -1 || sign == 1);

  const int tz_digits_index = components.IndexOf(MatchType_DIGITS);
  if (tz_digits_index < 0) {
    return;
  }
  const DigitsMatch* tz_digits =
      components.SubmatchAt<DigitsMatch>(tz_digits_index);
  if (tz_digits == nullptr) {
    return;
  }

  int offset;
  if (tz_digits->count_of_digits >= 3) {
    offset = (tz_digits->value / 100) * 60 + (tz_digits->value % 100);
  } else {
    offset = tz_digits->value * 60;
    if (const DigitsMatch* tz_digits_extra = components.SubmatchOf<DigitsMatch>(
            MatchType_DIGITS, /*start_index=*/tz_digits_index + 1)) {
      offset += tz_digits_extra->value;
    }
  }

  const NonterminalMatch* tz_offset =
      components.SubmatchOf<NonterminalMatch>(MatchType_NONTERMINAL);
  if (tz_offset == nullptr) {
    return;
  }

  const int time_zone_offset = sign * offset;
  if (!IsValidTimeZoneOffset(time_zone_offset)) {
    return;
  }

  TimeZoneOffsetMatch* result =
      matcher->AllocateAndInitMatch<TimeZoneOffsetMatch>(
          match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  result->time_zone_offset_param =
      tz_offset->nonterminal->time_zone_offset_parameter();
  result->time_zone_offset = time_zone_offset;
  matcher->AddMatch(result);
}

// Validates and adds a time zone name match.
void CheckTimeZoneName(const grammar::Match* match,
                       const NonterminalValue* nonterminal,
                       grammar::Matcher* matcher) {
  TC3_CHECK(match->IsUnaryRule());
  const TermValueMatch* tz_name =
      static_cast<const TermValueMatch*>(match->unary_rule_rhs());
  if (tz_name == nullptr) {
    return;
  }
  const TimeZoneNameSpec* tz_name_spec =
      tz_name->term_value->time_zone_name_spec();
  TimeZoneNameMatch* result = matcher->AllocateAndInitMatch<TimeZoneNameMatch>(
      match->lhs, match->codepoint_span, match->match_offset);
  result->Reset();
  result->nonterminal = nonterminal;
  result->time_zone_name_spec = tz_name_spec;
  result->time_zone_code = tz_name_spec->code();
  matcher->AddMatch(result);
}

// Adds a mapped term value match containing its value.
void AddTermValue(const grammar::Match* match, const TermValue* term_value,
                  grammar::Matcher* matcher) {
  TermValueMatch* term_match = matcher->AllocateAndInitMatch<TermValueMatch>(
      match->lhs, match->codepoint_span, match->match_offset);
  term_match->Reset();
  term_match->term_value = term_value;
  matcher->AddMatch(term_match);
}

// Adds a match for a nonterminal.
void AddNonterminal(const grammar::Match* match,
                    const NonterminalValue* nonterminal,
                    grammar::Matcher* matcher) {
  NonterminalMatch* result =
      matcher->AllocateAndInitMatch<NonterminalMatch>(*match);
  result->Reset();
  result->nonterminal = nonterminal;
  matcher->AddMatch(result);
}

// Adds a match for an extraction rule that is potentially used in a date range
// rule.
void AddExtractionRuleMatch(const grammar::Match* match,
                            const ExtractionRuleParameter* rule,
                            grammar::Matcher* matcher) {
  ExtractionMatch* result =
      matcher->AllocateAndInitMatch<ExtractionMatch>(*match);
  result->Reset();
  result->extraction_rule = rule;
  matcher->AddMatch(result);
}

}  // namespace

void DateExtractor::HandleExtractionRuleMatch(
    const ExtractionRuleParameter* rule, const grammar::Match* match,
    grammar::Matcher* matcher) {
  if (rule->id() != nullptr) {
    const std::string rule_id = rule->id()->str();
    bool keep = false;
    for (const std::string& extra_requested_dates_id :
         options_.extra_requested_dates) {
      if (extra_requested_dates_id == rule_id) {
        keep = true;
        break;
      }
    }
    if (!keep) {
      return;
    }
  }
  output_.push_back(
      Output{rule, matcher->AllocateAndInitMatch<grammar::Match>(*match)});
}

void DateExtractor::HandleRangeExtractionRuleMatch(const grammar::Match* match,
                                                   grammar::Matcher* matcher) {
  // Collect the two datetime roots that make up the range.
  std::vector<const grammar::Match*> parts;
  grammar::Traverse(match, [match, &parts](const grammar::Match* node) {
    if (node == match || node->type == grammar::Match::kUnknownType) {
      // Just continue traversing the match.
      return true;
    }

    // Collect, but don't expand the individual datetime nodes.
    parts.push_back(node);
    return false;
  });
  TC3_CHECK_EQ(parts.size(), 2);
  range_output_.push_back(
      RangeOutput{matcher->AllocateAndInitMatch<grammar::Match>(*match),
                  /*from=*/parts[0], /*to=*/parts[1]});
}

void DateExtractor::MatchFound(const grammar::Match* match,
                               const grammar::CallbackId type,
                               const int64 value, grammar::Matcher* matcher) {
  switch (type) {
    case MatchType_DATETIME_RULE: {
      HandleExtractionRuleMatch(
          /*rule=*/
          datetime_rules_->extraction_rule()->Get(value), match, matcher);
      return;
    }
    case MatchType_DATETIME_RANGE_RULE: {
      HandleRangeExtractionRuleMatch(match, matcher);
      return;
    }
    case MatchType_DATETIME: {
      // If an extraction rule is also part of a range extraction rule, then the
      // extraction rule is treated as a rule match and nonterminal match.
      // This type is used to match the rule as non terminal.
      AddExtractionRuleMatch(
          match, datetime_rules_->extraction_rule()->Get(value), matcher);
      return;
    }
    case MatchType_TERM_VALUE: {
      // Handle mapped terms.
      AddTermValue(match, datetime_rules_->term_value()->Get(value), matcher);
      return;
    }
    default:
      break;
  }

  // Handle non-terminals.
  const NonterminalValue* nonterminal =
      datetime_rules_->nonterminal_value()->Get(value);
  StringPiece match_text =
      StringPiece(text_[match->codepoint_span.first].utf8_data(),
                  text_[match->codepoint_span.second].utf8_data() -
                      text_[match->codepoint_span.first].utf8_data());
  switch (type) {
    case MatchType_NONTERMINAL:
      AddNonterminal(match, nonterminal, matcher);
      break;
    case MatchType_DIGITS:
      CheckDigits<DigitsMatch>(match, nonterminal, match_text, matcher);
      break;
    case MatchType_YEAR:
      CheckDigits<YearMatch>(match, nonterminal, match_text, matcher);
      break;
    case MatchType_MONTH:
      CheckAndAddNumericValue<MonthMatch>(match, nonterminal, match_text,
                                          matcher);
      break;
    case MatchType_DAY:
      CheckAndAddNumericValue<DayMatch>(match, nonterminal, match_text,
                                        matcher);
      break;
    case MatchType_DAY_OF_WEEK:
      CheckAndAddDirectOrMappedValue<DayOfWeekMatch>(match, nonterminal,
                                                     matcher);
      break;
    case MatchType_HOUR:
      CheckAndAddNumericValue<HourMatch>(match, nonterminal, match_text,
                                         matcher);
      break;
    case MatchType_MINUTE:
      CheckAndAddNumericValue<MinuteMatch>(match, nonterminal, match_text,
                                           matcher);
      break;
    case MatchType_SECOND:
      CheckAndAddNumericValue<SecondMatch>(match, nonterminal, match_text,
                                           matcher);
      break;
    case MatchType_FRACTION_SECOND:
      CheckDigitsAsFraction<FractionSecondMatch>(match, nonterminal, match_text,
                                                 matcher);
      break;
    case MatchType_TIME_VALUE:
      CheckTimeValue(text_, match, nonterminal, matcher);
      break;
    case MatchType_TIME_SPAN:
      CheckTimeSpan(match, nonterminal, matcher);
      break;
    case MatchType_TIME_ZONE_NAME:
      CheckTimeZoneName(match, nonterminal, matcher);
      break;
    case MatchType_TIME_ZONE_OFFSET:
      CheckTimeZoneOffset(match, nonterminal, matcher);
      break;
    case MatchType_TIME_PERIOD:
      CheckTimePeriod(text_, match, nonterminal, matcher);
      break;
    case MatchType_RELATIVE_DATE:
      CheckRelativeDate(options_, match, nonterminal, matcher);
      break;
    case MatchType_COMBINED_DIGITS:
      CheckCombinedDigits<CombinedDigitsMatch>(match, nonterminal, match_text,
                                               matcher);
      break;
    default:
      TC3_VLOG(ERROR) << "Unhandled match type: " << type;
  }
}

}  // namespace libtextclassifier3::dates
