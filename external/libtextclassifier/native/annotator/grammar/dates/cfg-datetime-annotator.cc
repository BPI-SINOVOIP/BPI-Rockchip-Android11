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

#include "annotator/grammar/dates/cfg-datetime-annotator.h"

#include "annotator/datetime/utils.h"
#include "annotator/grammar/dates/annotations/annotation-options.h"
#include "annotator/grammar/utils.h"
#include "utils/strings/split.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3::dates {
namespace {

static std::string GetReferenceLocale(const std::string& locales) {
  std::vector<StringPiece> split_locales = strings::Split(locales, ',');
  if (!split_locales.empty()) {
    return split_locales[0].ToString();
  }
  return "";
}

static void InterpretParseData(const DatetimeParsedData& datetime_parsed_data,
                               const DateAnnotationOptions& options,
                               const CalendarLib& calendarlib,
                               int64* interpreted_time_ms_utc,
                               DatetimeGranularity* granularity) {
  DatetimeGranularity local_granularity =
      calendarlib.GetGranularity(datetime_parsed_data);
  if (!calendarlib.InterpretParseData(
          datetime_parsed_data, options.base_timestamp_millis,
          options.reference_timezone, GetReferenceLocale(options.locales),
          /*prefer_future_for_unspecified_date=*/true, interpreted_time_ms_utc,
          granularity)) {
    TC3_LOG(WARNING) << "Failed to extract time in millis and Granularity.";
    // Fallingback to DatetimeParsedData's finest granularity
    *granularity = local_granularity;
  }
}

}  // namespace

CfgDatetimeAnnotator::CfgDatetimeAnnotator(
    const UniLib* unilib, const GrammarTokenizerOptions* tokenizer_options,
    const CalendarLib* calendar_lib, const DatetimeRules* datetime_rules,
    const float annotator_target_classification_score,
    const float annotator_priority_score)
    : calendar_lib_(*calendar_lib),
      tokenizer_(BuildTokenizer(unilib, tokenizer_options)),
      parser_(unilib, datetime_rules),
      annotator_target_classification_score_(
          annotator_target_classification_score),
      annotator_priority_score_(annotator_priority_score) {}

void CfgDatetimeAnnotator::Parse(
    const std::string& input, const DateAnnotationOptions& annotation_options,
    const std::vector<Locale>& locales,
    std::vector<DatetimeParseResultSpan>* results) const {
  Parse(UTF8ToUnicodeText(input, /*do_copy=*/false), annotation_options,
        locales, results);
}

void CfgDatetimeAnnotator::ProcessDatetimeParseResult(
    const DateAnnotationOptions& annotation_options,
    const DatetimeParseResult& datetime_parse_result,
    std::vector<DatetimeParseResult>* results) const {
  DatetimeParsedData datetime_parsed_data;
  datetime_parsed_data.AddDatetimeComponents(
      datetime_parse_result.datetime_components);

  std::vector<DatetimeParsedData> interpretations;
  if (annotation_options.generate_alternative_interpretations_when_ambiguous) {
    FillInterpretations(datetime_parsed_data,
                        calendar_lib_.GetGranularity(datetime_parsed_data),
                        &interpretations);
  } else {
    interpretations.emplace_back(datetime_parsed_data);
  }
  for (const DatetimeParsedData& interpretation : interpretations) {
    results->emplace_back();
    interpretation.GetDatetimeComponents(&results->back().datetime_components);
    InterpretParseData(interpretation, annotation_options, calendar_lib_,
                       &(results->back().time_ms_utc),
                       &(results->back().granularity));
    std::sort(results->back().datetime_components.begin(),
              results->back().datetime_components.end(),
              [](const DatetimeComponent& a, const DatetimeComponent& b) {
                return a.component_type > b.component_type;
              });
  }
}

void CfgDatetimeAnnotator::Parse(
    const UnicodeText& input, const DateAnnotationOptions& annotation_options,
    const std::vector<Locale>& locales,
    std::vector<DatetimeParseResultSpan>* results) const {
  std::vector<DatetimeParseResultSpan> grammar_datetime_parse_result_spans =
      parser_.Parse(input.data(), tokenizer_.Tokenize(input), locales,
                    annotation_options);

  for (const DatetimeParseResultSpan& grammar_datetime_parse_result_span :
       grammar_datetime_parse_result_spans) {
    DatetimeParseResultSpan datetime_parse_result_span;
    datetime_parse_result_span.span.first =
        grammar_datetime_parse_result_span.span.first;
    datetime_parse_result_span.span.second =
        grammar_datetime_parse_result_span.span.second;
    datetime_parse_result_span.priority_score = annotator_priority_score_;
    if (annotation_options.use_rule_priority_score) {
      datetime_parse_result_span.priority_score =
          grammar_datetime_parse_result_span.priority_score;
    }
    datetime_parse_result_span.target_classification_score =
        annotator_target_classification_score_;
    for (const DatetimeParseResult& grammar_datetime_parse_result :
         grammar_datetime_parse_result_span.data) {
      ProcessDatetimeParseResult(annotation_options,
                                 grammar_datetime_parse_result,
                                 &datetime_parse_result_span.data);
    }
    results->emplace_back(datetime_parse_result_span);
  }
}

}  // namespace libtextclassifier3::dates
