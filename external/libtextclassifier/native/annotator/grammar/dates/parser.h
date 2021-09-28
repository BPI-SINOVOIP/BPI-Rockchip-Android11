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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_PARSER_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_PARSER_H_

#include <vector>

#include "annotator/grammar/dates/annotations/annotation-options.h"
#include "annotator/grammar/dates/annotations/annotation.h"
#include "annotator/grammar/dates/dates_generated.h"
#include "annotator/grammar/dates/utils/date-match.h"
#include "utils/grammar/lexer.h"
#include "utils/grammar/rules-utils.h"
#include "utils/i18n/locale.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3::dates {

// Parses datetime expressions in the input with the datetime grammar and
// constructs, validates, deduplicates and normalizes date time annotations.
class DateParser {
 public:
  explicit DateParser(const UniLib* unilib, const DatetimeRules* datetime_rules)
      : unilib_(*unilib),
        lexer_(unilib, datetime_rules->rules()),
        datetime_rules_(datetime_rules),
        rules_locales_(ParseRulesLocales(datetime_rules->rules())) {}

  // Parses the dates in the input. Makes sure that the results do not
  // overlap.
  std::vector<DatetimeParseResultSpan> Parse(
      StringPiece text, const std::vector<Token>& tokens,
      const std::vector<Locale>& locales,
      const DateAnnotationOptions& options) const;

 private:
  const UniLib& unilib_;
  const grammar::Lexer lexer_;

  // The datetime grammar.
  const DatetimeRules* datetime_rules_;

  // Pre-parsed locales of the rules.
  const std::vector<std::vector<Locale>> rules_locales_;
};

}  // namespace libtextclassifier3::dates

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_PARSER_H_
