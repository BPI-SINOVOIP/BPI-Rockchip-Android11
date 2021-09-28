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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_EXTRACTOR_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_EXTRACTOR_H_

#include <vector>

#include "annotator/grammar/dates/annotations/annotation-options.h"
#include "annotator/grammar/dates/dates_generated.h"
#include "utils/base/integral_types.h"
#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/match.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/types.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3::dates {

// A helper class for the datetime parser that extracts structured data from
// the datetime grammar matches.
// It handles simple sanity checking of the rule matches and interacts with the
// grammar matcher to extract all datetime occurrences in a text.
class DateExtractor : public grammar::CallbackDelegate {
 public:
  // Represents a date match for an extraction rule.
  struct Output {
    const ExtractionRuleParameter* rule = nullptr;
    const grammar::Match* match = nullptr;
  };

  // Represents a date match from a range extraction rule.
  struct RangeOutput {
    const grammar::Match* match = nullptr;
    const grammar::Match* from = nullptr;
    const grammar::Match* to = nullptr;
  };

  DateExtractor(const std::vector<UnicodeText::const_iterator>& text,
                const DateAnnotationOptions& options,
                const DatetimeRules* datetime_rules)
      : text_(text), options_(options), datetime_rules_(datetime_rules) {}

  // Handle a rule match in the date time grammar.
  // This checks the type of the match and does type dependent checks.
  void MatchFound(const grammar::Match* match, grammar::CallbackId type,
                  int64 value, grammar::Matcher* matcher) override;

  const std::vector<Output>& output() const { return output_; }
  const std::vector<RangeOutput>& range_output() const { return range_output_; }

 private:
  // Extracts a date from a root rule match.
  void HandleExtractionRuleMatch(const ExtractionRuleParameter* rule,
                                 const grammar::Match* match,
                                 grammar::Matcher* matcher);

  // Extracts a date range from a root rule match.
  void HandleRangeExtractionRuleMatch(const grammar::Match* match,
                                      grammar::Matcher* matcher);

  const std::vector<UnicodeText::const_iterator>& text_;
  const DateAnnotationOptions& options_;
  const DatetimeRules* datetime_rules_;

  // Extraction results.
  std::vector<Output> output_;
  std::vector<RangeOutput> range_output_;
};

}  // namespace libtextclassifier3::dates

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_EXTRACTOR_H_
