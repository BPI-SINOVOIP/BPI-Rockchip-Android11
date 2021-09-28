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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_DURATION_DURATION_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_DURATION_DURATION_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "annotator/feature-processor.h"
#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

namespace internal {
enum class DurationUnit {
  UNKNOWN = -1,
  WEEK = 0,
  DAY = 1,
  HOUR = 2,
  MINUTE = 3,
  SECOND = 4

  // NOTE: If we want to add MONTH and YEAR we'll have to think of different
  // parsing format, because MONTH and YEAR don't have a fixed number of
  // milliseconds, unlike week/day/hour/minute/second. We ignore the daylight
  // savings time and assume the day is always 24 hours.
};

// Prepares the mapping between token values and duration unit types.
std::unordered_map<std::string, internal::DurationUnit>
BuildTokenToDurationUnitMapping(const DurationAnnotatorOptions* options,
                                const UniLib* unilib);

// Creates a set of strings from a flatbuffer string vector.
std::unordered_set<std::string> BuildStringSet(
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*
        strings,
    const UniLib* unilib);

// Creates a set of ints from a flatbuffer int vector.
std::unordered_set<int32> BuildInt32Set(const flatbuffers::Vector<int32>* ints);

}  // namespace internal

// Annotator of duration expressions like "3 minutes 30 seconds".
class DurationAnnotator {
 public:
  explicit DurationAnnotator(const DurationAnnotatorOptions* options,
                             const FeatureProcessor* feature_processor,
                             const UniLib* unilib)
      : options_(options),
        feature_processor_(feature_processor),
        unilib_(unilib),
        token_value_to_duration_unit_(
            internal::BuildTokenToDurationUnitMapping(options, unilib)),
        filler_expressions_(
            internal::BuildStringSet(options->filler_expressions(), unilib)),
        half_expressions_(
            internal::BuildStringSet(options->half_expressions(), unilib)),
        sub_token_separator_codepoints_(internal::BuildInt32Set(
            options->sub_token_separator_codepoints())) {}

  // Classifies given text, and if it is a duration, it passes the result in
  // 'classification_result' and returns true, otherwise returns false.
  bool ClassifyText(const UnicodeText& context, CodepointSpan selection_indices,
                    AnnotationUsecase annotation_usecase,
                    ClassificationResult* classification_result) const;

  // Finds all duration instances in the input text.
  bool FindAll(const UnicodeText& context, const std::vector<Token>& tokens,
               AnnotationUsecase annotation_usecase,
               std::vector<AnnotatedSpan>* results) const;

 private:
  // Represents a component of duration parsed from text (e.g. "3 hours" from
  // the expression "3 hours and 20 minutes").
  struct ParsedDurationAtom {
    // Unit of the duration.
    internal::DurationUnit unit = internal::DurationUnit::UNKNOWN;

    // Quantity of the duration unit.
    int value = 0;

    // True, if half an unit was specified (either in addition, or exclusively).
    // E.g. "hour and a half".
    // NOTE: Quarter, three-quarters etc. is not supported.
    bool plus_half = false;

    static ParsedDurationAtom Half() {
      ParsedDurationAtom result;
      result.plus_half = true;
      return result;
    }
  };

  // Starts consuming tokens and returns the index past the last consumed token.
  int FindDurationStartingAt(const UnicodeText& context,
                             const std::vector<Token>& tokens,
                             int start_token_index,
                             AnnotatedSpan* result) const;

  bool ParseQuantityToken(const Token& token, ParsedDurationAtom* value) const;
  bool ParseDurationUnitToken(const Token& token,
                              internal::DurationUnit* duration_unit) const;
  bool ParseQuantityDurationUnitToken(const Token& token,
                                      ParsedDurationAtom* value) const;
  bool ParseFillerToken(const Token& token) const;

  int64 ParsedDurationAtomsToMillis(
      const std::vector<ParsedDurationAtom>& atoms) const;

  const DurationAnnotatorOptions* options_;
  const FeatureProcessor* feature_processor_;
  const UniLib* unilib_;
  const std::unordered_map<std::string, internal::DurationUnit>
      token_value_to_duration_unit_;
  const std::unordered_set<std::string> filler_expressions_;
  const std::unordered_set<std::string> half_expressions_;
  const std::unordered_set<int32> sub_token_separator_codepoints_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_DURATION_DURATION_H_
