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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_CFG_DATETIME_ANNOTATOR_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_CFG_DATETIME_ANNOTATOR_H_

#include "annotator/grammar/dates/annotations/annotation.h"
#include "annotator/grammar/dates/dates_generated.h"
#include "annotator/grammar/dates/parser.h"
#include "annotator/grammar/dates/utils/annotation-keys.h"
#include "annotator/model_generated.h"
#include "utils/calendar/calendar.h"
#include "utils/i18n/locale.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3::dates {

// Helper class to convert the parsed datetime expression from AnnotationList
// (List of annotation generated from Grammar rules) to DatetimeParseResultSpan.
class CfgDatetimeAnnotator {
 public:
  explicit CfgDatetimeAnnotator(
      const UniLib* unilib, const GrammarTokenizerOptions* tokenizer_options,
      const CalendarLib* calendar_lib, const DatetimeRules* datetime_rules,
      const float annotator_target_classification_score,
      const float annotator_priority_score);

  // CfgDatetimeAnnotator is neither copyable nor movable.
  CfgDatetimeAnnotator(const CfgDatetimeAnnotator&) = delete;
  CfgDatetimeAnnotator& operator=(const CfgDatetimeAnnotator&) = delete;

  // Parses the dates in 'input' and fills result. Makes sure that the results
  // do not overlap.
  // Method will return false if input does not contain any datetime span.
  void Parse(const std::string& input,
             const DateAnnotationOptions& annotation_options,
             const std::vector<Locale>& locales,
             std::vector<DatetimeParseResultSpan>* results) const;

  // UnicodeText version of parse.
  void Parse(const UnicodeText& input,
             const DateAnnotationOptions& annotation_options,
             const std::vector<Locale>& locales,
             std::vector<DatetimeParseResultSpan>* results) const;

 private:
  void ProcessDatetimeParseResult(
      const DateAnnotationOptions& annotation_options,
      const DatetimeParseResult& datetime_parse_result,
      std::vector<DatetimeParseResult>* results) const;

  const CalendarLib& calendar_lib_;
  const Tokenizer tokenizer_;
  DateParser parser_;
  const float annotator_target_classification_score_;
  const float annotator_priority_score_;
};

}  // namespace libtextclassifier3::dates
#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_CFG_DATETIME_ANNOTATOR_H_
