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

#include "annotator/datetime/parser.h"

#include <set>
#include <unordered_set>

#include "annotator/datetime/extractor.h"
#include "annotator/datetime/utils.h"
#include "utils/calendar/calendar.h"
#include "utils/i18n/locale.h"
#include "utils/strings/split.h"
#include "utils/zlib/zlib_regex.h"

namespace libtextclassifier3 {
std::unique_ptr<DatetimeParser> DatetimeParser::Instance(
    const DatetimeModel* model, const UniLib* unilib,
    const CalendarLib* calendarlib, ZlibDecompressor* decompressor) {
  std::unique_ptr<DatetimeParser> result(
      new DatetimeParser(model, unilib, calendarlib, decompressor));
  if (!result->initialized_) {
    result.reset();
  }
  return result;
}

DatetimeParser::DatetimeParser(const DatetimeModel* model, const UniLib* unilib,
                               const CalendarLib* calendarlib,
                               ZlibDecompressor* decompressor)
    : unilib_(*unilib), calendarlib_(*calendarlib) {
  initialized_ = false;

  if (model == nullptr) {
    return;
  }

  if (model->patterns() != nullptr) {
    for (const DatetimeModelPattern* pattern : *model->patterns()) {
      if (pattern->regexes()) {
        for (const DatetimeModelPattern_::Regex* regex : *pattern->regexes()) {
          std::unique_ptr<UniLib::RegexPattern> regex_pattern =
              UncompressMakeRegexPattern(
                  unilib_, regex->pattern(), regex->compressed_pattern(),
                  model->lazy_regex_compilation(), decompressor);
          if (!regex_pattern) {
            TC3_LOG(ERROR) << "Couldn't create rule pattern.";
            return;
          }
          rules_.push_back({std::move(regex_pattern), regex, pattern});
          if (pattern->locales()) {
            for (int locale : *pattern->locales()) {
              locale_to_rules_[locale].push_back(rules_.size() - 1);
            }
          }
        }
      }
    }
  }

  if (model->extractors() != nullptr) {
    for (const DatetimeModelExtractor* extractor : *model->extractors()) {
      std::unique_ptr<UniLib::RegexPattern> regex_pattern =
          UncompressMakeRegexPattern(
              unilib_, extractor->pattern(), extractor->compressed_pattern(),
              model->lazy_regex_compilation(), decompressor);
      if (!regex_pattern) {
        TC3_LOG(ERROR) << "Couldn't create extractor pattern";
        return;
      }
      extractor_rules_.push_back(std::move(regex_pattern));

      if (extractor->locales()) {
        for (int locale : *extractor->locales()) {
          type_and_locale_to_extractor_rule_[extractor->extractor()][locale] =
              extractor_rules_.size() - 1;
        }
      }
    }
  }

  if (model->locales() != nullptr) {
    for (int i = 0; i < model->locales()->size(); ++i) {
      locale_string_to_id_[model->locales()->Get(i)->str()] = i;
    }
  }

  if (model->default_locales() != nullptr) {
    for (const int locale : *model->default_locales()) {
      default_locale_ids_.push_back(locale);
    }
  }

  use_extractors_for_locating_ = model->use_extractors_for_locating();
  generate_alternative_interpretations_when_ambiguous_ =
      model->generate_alternative_interpretations_when_ambiguous();
  prefer_future_for_unspecified_date_ =
      model->prefer_future_for_unspecified_date();

  initialized_ = true;
}

bool DatetimeParser::Parse(
    const std::string& input, const int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::string& locales,
    ModeFlag mode, AnnotationUsecase annotation_usecase, bool anchor_start_end,
    std::vector<DatetimeParseResultSpan>* results) const {
  return Parse(UTF8ToUnicodeText(input, /*do_copy=*/false),
               reference_time_ms_utc, reference_timezone, locales, mode,
               annotation_usecase, anchor_start_end, results);
}

bool DatetimeParser::FindSpansUsingLocales(
    const std::vector<int>& locale_ids, const UnicodeText& input,
    const int64 reference_time_ms_utc, const std::string& reference_timezone,
    ModeFlag mode, AnnotationUsecase annotation_usecase, bool anchor_start_end,
    const std::string& reference_locale,
    std::unordered_set<int>* executed_rules,
    std::vector<DatetimeParseResultSpan>* found_spans) const {
  for (const int locale_id : locale_ids) {
    auto rules_it = locale_to_rules_.find(locale_id);
    if (rules_it == locale_to_rules_.end()) {
      continue;
    }

    for (const int rule_id : rules_it->second) {
      // Skip rules that were already executed in previous locales.
      if (executed_rules->find(rule_id) != executed_rules->end()) {
        continue;
      }

      if ((rules_[rule_id].pattern->enabled_annotation_usecases() &
           (1 << annotation_usecase)) == 0) {
        continue;
      }

      if (!(rules_[rule_id].pattern->enabled_modes() & mode)) {
        continue;
      }

      executed_rules->insert(rule_id);

      if (!ParseWithRule(rules_[rule_id], input, reference_time_ms_utc,
                         reference_timezone, reference_locale, locale_id,
                         anchor_start_end, found_spans)) {
        return false;
      }
    }
  }
  return true;
}

bool DatetimeParser::Parse(
    const UnicodeText& input, const int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::string& locales,
    ModeFlag mode, AnnotationUsecase annotation_usecase, bool anchor_start_end,
    std::vector<DatetimeParseResultSpan>* results) const {
  std::vector<DatetimeParseResultSpan> found_spans;
  std::unordered_set<int> executed_rules;
  std::string reference_locale;
  const std::vector<int> requested_locales =
      ParseAndExpandLocales(locales, &reference_locale);
  if (!FindSpansUsingLocales(requested_locales, input, reference_time_ms_utc,
                             reference_timezone, mode, annotation_usecase,
                             anchor_start_end, reference_locale,
                             &executed_rules, &found_spans)) {
    return false;
  }

  std::vector<std::pair<DatetimeParseResultSpan, int>> indexed_found_spans;
  indexed_found_spans.reserve(found_spans.size());
  for (int i = 0; i < found_spans.size(); i++) {
    indexed_found_spans.push_back({found_spans[i], i});
  }

  // Resolve conflicts by always picking the longer span and breaking ties by
  // selecting the earlier entry in the list for a given locale.
  std::sort(indexed_found_spans.begin(), indexed_found_spans.end(),
            [](const std::pair<DatetimeParseResultSpan, int>& a,
               const std::pair<DatetimeParseResultSpan, int>& b) {
              if ((a.first.span.second - a.first.span.first) !=
                  (b.first.span.second - b.first.span.first)) {
                return (a.first.span.second - a.first.span.first) >
                       (b.first.span.second - b.first.span.first);
              } else {
                return a.second < b.second;
              }
            });

  found_spans.clear();
  for (auto& span_index_pair : indexed_found_spans) {
    found_spans.push_back(span_index_pair.first);
  }

  std::set<int, std::function<bool(int, int)>> chosen_indices_set(
      [&found_spans](int a, int b) {
        return found_spans[a].span.first < found_spans[b].span.first;
      });
  for (int i = 0; i < found_spans.size(); ++i) {
    if (!DoesCandidateConflict(i, found_spans, chosen_indices_set)) {
      chosen_indices_set.insert(i);
      results->push_back(found_spans[i]);
    }
  }

  return true;
}

bool DatetimeParser::HandleParseMatch(
    const CompiledRule& rule, const UniLib::RegexMatcher& matcher,
    int64 reference_time_ms_utc, const std::string& reference_timezone,
    const std::string& reference_locale, int locale_id,
    std::vector<DatetimeParseResultSpan>* result) const {
  int status = UniLib::RegexMatcher::kNoError;
  const int start = matcher.Start(&status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }

  const int end = matcher.End(&status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }

  DatetimeParseResultSpan parse_result;
  std::vector<DatetimeParseResult> alternatives;
  if (!ExtractDatetime(rule, matcher, reference_time_ms_utc, reference_timezone,
                       reference_locale, locale_id, &alternatives,
                       &parse_result.span)) {
    return false;
  }

  if (!use_extractors_for_locating_) {
    parse_result.span = {start, end};
  }

  if (parse_result.span.first != kInvalidIndex &&
      parse_result.span.second != kInvalidIndex) {
    parse_result.target_classification_score =
        rule.pattern->target_classification_score();
    parse_result.priority_score = rule.pattern->priority_score();

    for (DatetimeParseResult& alternative : alternatives) {
      parse_result.data.push_back(alternative);
    }
  }
  result->push_back(parse_result);
  return true;
}

bool DatetimeParser::ParseWithRule(
    const CompiledRule& rule, const UnicodeText& input,
    const int64 reference_time_ms_utc, const std::string& reference_timezone,
    const std::string& reference_locale, const int locale_id,
    bool anchor_start_end, std::vector<DatetimeParseResultSpan>* result) const {
  std::unique_ptr<UniLib::RegexMatcher> matcher =
      rule.compiled_regex->Matcher(input);
  int status = UniLib::RegexMatcher::kNoError;
  if (anchor_start_end) {
    if (matcher->Matches(&status) && status == UniLib::RegexMatcher::kNoError) {
      if (!HandleParseMatch(rule, *matcher, reference_time_ms_utc,
                            reference_timezone, reference_locale, locale_id,
                            result)) {
        return false;
      }
    }
  } else {
    while (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
      if (!HandleParseMatch(rule, *matcher, reference_time_ms_utc,
                            reference_timezone, reference_locale, locale_id,
                            result)) {
        return false;
      }
    }
  }
  return true;
}

std::vector<int> DatetimeParser::ParseAndExpandLocales(
    const std::string& locales, std::string* reference_locale) const {
  std::vector<StringPiece> split_locales = strings::Split(locales, ',');
  if (!split_locales.empty()) {
    *reference_locale = split_locales[0].ToString();
  } else {
    *reference_locale = "";
  }

  std::vector<int> result;
  for (const StringPiece& locale_str : split_locales) {
    auto locale_it = locale_string_to_id_.find(locale_str.ToString());
    if (locale_it != locale_string_to_id_.end()) {
      result.push_back(locale_it->second);
    }

    const Locale locale = Locale::FromBCP47(locale_str.ToString());
    if (!locale.IsValid()) {
      continue;
    }

    const std::string language = locale.Language();
    const std::string script = locale.Script();
    const std::string region = locale.Region();

    // First, try adding *-region locale.
    if (!region.empty()) {
      locale_it = locale_string_to_id_.find("*-" + region);
      if (locale_it != locale_string_to_id_.end()) {
        result.push_back(locale_it->second);
      }
    }
    // Second, try adding language-script-* locale.
    if (!script.empty()) {
      locale_it = locale_string_to_id_.find(language + "-" + script + "-*");
      if (locale_it != locale_string_to_id_.end()) {
        result.push_back(locale_it->second);
      }
    }
    // Third, try adding language-* locale.
    if (!language.empty()) {
      locale_it = locale_string_to_id_.find(language + "-*");
      if (locale_it != locale_string_to_id_.end()) {
        result.push_back(locale_it->second);
      }
    }
  }

  // Add the default locales if they haven't been added already.
  const std::unordered_set<int> result_set(result.begin(), result.end());
  for (const int default_locale_id : default_locale_ids_) {
    if (result_set.find(default_locale_id) == result_set.end()) {
      result.push_back(default_locale_id);
    }
  }

  return result;
}

bool DatetimeParser::ExtractDatetime(const CompiledRule& rule,
                                     const UniLib::RegexMatcher& matcher,
                                     const int64 reference_time_ms_utc,
                                     const std::string& reference_timezone,
                                     const std::string& reference_locale,
                                     int locale_id,
                                     std::vector<DatetimeParseResult>* results,
                                     CodepointSpan* result_span) const {
  DatetimeParsedData parse;
  DatetimeExtractor extractor(rule, matcher, locale_id, &unilib_,
                              extractor_rules_,
                              type_and_locale_to_extractor_rule_);
  if (!extractor.Extract(&parse, result_span)) {
    return false;
  }
  std::vector<DatetimeParsedData> interpretations;
  if (generate_alternative_interpretations_when_ambiguous_) {
    FillInterpretations(parse, calendarlib_.GetGranularity(parse),
                        &interpretations);
  } else {
    interpretations.push_back(parse);
  }

  results->reserve(results->size() + interpretations.size());
  for (const DatetimeParsedData& interpretation : interpretations) {
    std::vector<DatetimeComponent> date_components;
    interpretation.GetDatetimeComponents(&date_components);
    DatetimeParseResult result;
    // TODO(hassan): Text classifier only provides ambiguity limited to “AM/PM”
    //               which is encoded in the pair of DatetimeParseResult; both
    //               corresponding to the same date, but one corresponding to
    //               “AM” and the other one corresponding to “PM”.
    //               Remove multiple DatetimeParseResult per datetime span,
    //               once the ambiguities/DatetimeComponents are added in the
    //               response. For Details see b/130355975
    if (!calendarlib_.InterpretParseData(
            interpretation, reference_time_ms_utc, reference_timezone,
            reference_locale, prefer_future_for_unspecified_date_,
            &(result.time_ms_utc), &(result.granularity))) {
      return false;
    }

    // Sort the date time units by component type.
    std::sort(date_components.begin(), date_components.end(),
              [](DatetimeComponent a, DatetimeComponent b) {
                return a.component_type > b.component_type;
              });
    result.datetime_components.swap(date_components);
    results->push_back(result);
  }
  return true;
}

}  // namespace libtextclassifier3
