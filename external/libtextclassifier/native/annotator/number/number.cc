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

#include "annotator/number/number.h"

#include <climits>
#include <cstdlib>
#include <string>

#include "annotator/collections.h"
#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/strings/split.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

bool NumberAnnotator::ClassifyText(
    const UnicodeText& context, CodepointSpan selection_indices,
    AnnotationUsecase annotation_usecase,
    ClassificationResult* classification_result) const {
  TC3_CHECK(classification_result != nullptr);

  const UnicodeText substring_selected = UnicodeText::Substring(
      context, selection_indices.first, selection_indices.second);

  std::vector<AnnotatedSpan> results;
  if (!FindAll(substring_selected, annotation_usecase, &results)) {
    return false;
  }

  for (const AnnotatedSpan& result : results) {
    if (result.classification.empty()) {
      continue;
    }

    // We make sure that the result span is equal to the stripped selection span
    // to avoid validating cases like "23 asdf 3.14 pct asdf". FindAll will
    // anyway only find valid numbers and percentages and a given selection with
    // more than two tokens won't pass this check.
    if (result.span.first + selection_indices.first ==
            selection_indices.first &&
        result.span.second + selection_indices.first ==
            selection_indices.second) {
      *classification_result = result.classification[0];
      return true;
    }
  }
  return false;
}

bool NumberAnnotator::IsCJTterm(UnicodeText::const_iterator token_begin_it,
                                const int token_length) const {
  auto token_end_it = token_begin_it;
  std::advance(token_end_it, token_length);
  for (auto char_it = token_begin_it; char_it < token_end_it; ++char_it) {
    if (!unilib_->IsCJTletter(*char_it)) {
      return false;
    }
  }
  return true;
}

bool NumberAnnotator::TokensAreValidStart(const std::vector<Token>& tokens,
                                          const int start_index) const {
  if (start_index < 0 || tokens[start_index].is_whitespace) {
    return true;
  }
  return false;
}

bool NumberAnnotator::TokensAreValidNumberPrefix(
    const std::vector<Token>& tokens, const int prefix_end_index) const {
  if (TokensAreValidStart(tokens, prefix_end_index)) {
    return true;
  }

  auto prefix_begin_it =
      UTF8ToUnicodeText(tokens[prefix_end_index].value, /*do_copy=*/false)
          .begin();
  const int token_length =
      tokens[prefix_end_index].end - tokens[prefix_end_index].start;
  if (token_length == 1 && unilib_->IsOpeningBracket(*prefix_begin_it) &&
      TokensAreValidStart(tokens, prefix_end_index - 1)) {
    return true;
  }
  if (token_length == 1 && unilib_->IsNumberSign(*prefix_begin_it) &&
      TokensAreValidStart(tokens, prefix_end_index - 1)) {
    return true;
  }
  if (token_length == 1 && unilib_->IsSlash(*prefix_begin_it) &&
      prefix_end_index >= 1 &&
      TokensAreValidStart(tokens, prefix_end_index - 2)) {
    int64 int_val;
    double double_val;
    return TryParseNumber(UTF8ToUnicodeText(tokens[prefix_end_index - 1].value,
                                            /*do_copy=*/false),
                          false, &int_val, &double_val);
  }
  if (IsCJTterm(prefix_begin_it, token_length)) {
    return true;
  }

  return false;
}

bool NumberAnnotator::TokensAreValidEnding(const std::vector<Token>& tokens,
                                           const int ending_index) const {
  if (ending_index >= tokens.size() || tokens[ending_index].is_whitespace) {
    return true;
  }

  auto ending_begin_it =
      UTF8ToUnicodeText(tokens[ending_index].value, /*do_copy=*/false).begin();
  if (ending_index == tokens.size() - 1 &&
      tokens[ending_index].end - tokens[ending_index].start == 1 &&
      unilib_->IsPunctuation(*ending_begin_it)) {
    return true;
  }
  if (ending_index < tokens.size() - 1 &&
      tokens[ending_index].end - tokens[ending_index].start == 1 &&
      unilib_->IsPunctuation(*ending_begin_it) &&
      tokens[ending_index + 1].is_whitespace) {
    return true;
  }

  return false;
}

bool NumberAnnotator::TokensAreValidNumberSuffix(
    const std::vector<Token>& tokens, const int suffix_start_index) const {
  if (TokensAreValidEnding(tokens, suffix_start_index)) {
    return true;
  }

  auto suffix_begin_it =
      UTF8ToUnicodeText(tokens[suffix_start_index].value, /*do_copy=*/false)
          .begin();

  if (percent_suffixes_.find(tokens[suffix_start_index].value) !=
          percent_suffixes_.end() &&
      TokensAreValidEnding(tokens, suffix_start_index + 1)) {
    return true;
  }

  const int token_length =
      tokens[suffix_start_index].end - tokens[suffix_start_index].start;
  if (token_length == 1 && unilib_->IsSlash(*suffix_begin_it) &&
      suffix_start_index <= tokens.size() - 2 &&
      TokensAreValidEnding(tokens, suffix_start_index + 2)) {
    int64 int_val;
    double double_val;
    return TryParseNumber(
        UTF8ToUnicodeText(tokens[suffix_start_index + 1].value,
                          /*do_copy=*/false),
        false, &int_val, &double_val);
  }
  if (IsCJTterm(suffix_begin_it, token_length)) {
    return true;
  }

  return false;
}

int NumberAnnotator::FindPercentSuffixEndCodepoint(
    const std::vector<Token>& tokens,
    const int suffix_token_start_index) const {
  if (suffix_token_start_index >= tokens.size()) {
    return -1;
  }

  if (percent_suffixes_.find(tokens[suffix_token_start_index].value) !=
          percent_suffixes_.end() &&
      TokensAreValidEnding(tokens, suffix_token_start_index + 1)) {
    return tokens[suffix_token_start_index].end;
  }
  if (tokens[suffix_token_start_index].is_whitespace) {
    return FindPercentSuffixEndCodepoint(tokens, suffix_token_start_index + 1);
  }

  return -1;
}

bool NumberAnnotator::TryParseNumber(const UnicodeText& token_text,
                                     const bool is_negative,
                                     int64* parsed_int_value,
                                     double* parsed_double_value) const {
  if (token_text.ToUTF8String().size() >= max_number_of_digits_) {
    return false;
  }
  const bool is_double = unilib_->ParseDouble(token_text, parsed_double_value);
  if (!is_double) {
    return false;
  }
  *parsed_int_value = std::trunc(*parsed_double_value);
  if (is_negative) {
    *parsed_int_value *= -1;
    *parsed_double_value *= -1;
  }

  return true;
}

bool NumberAnnotator::FindAll(const UnicodeText& context,
                              AnnotationUsecase annotation_usecase,
                              std::vector<AnnotatedSpan>* result) const {
  if (!options_->enabled()) {
    return true;
  }

  const std::vector<Token> tokens = tokenizer_.Tokenize(context);
  for (int i = 0; i < tokens.size(); ++i) {
    const Token token = tokens[i];
    if (tokens[i].value.empty() ||
        !unilib_->IsDigit(
            *UTF8ToUnicodeText(tokens[i].value, /*do_copy=*/false).begin())) {
      continue;
    }

    const UnicodeText token_text =
        UTF8ToUnicodeText(token.value, /*do_copy=*/false);
    int64 parsed_int_value;
    double parsed_double_value;
    bool is_negative =
        (i > 0) &&
        unilib_->IsMinus(
            *UTF8ToUnicodeText(tokens[i - 1].value, /*do_copy=*/false).begin());
    if (!TryParseNumber(token_text, is_negative, &parsed_int_value,
                        &parsed_double_value)) {
      continue;
    }
    if (!TokensAreValidNumberPrefix(tokens, is_negative ? i - 2 : i - 1) ||
        !TokensAreValidNumberSuffix(tokens, i + 1)) {
      continue;
    }

    const bool has_decimal = !(parsed_int_value == parsed_double_value);
    const int new_start_codepoint = is_negative ? token.start - 1 : token.start;

    if (((1 << annotation_usecase) & options_->enabled_annotation_usecases()) !=
        0) {
      result->push_back(CreateAnnotatedSpan(
          new_start_codepoint, token.end, parsed_int_value, parsed_double_value,
          Collections::Number(), options_->score(),
          /*priority_score=*/
          has_decimal ? options_->float_number_priority_score()
                      : options_->priority_score()));
    }

    const int percent_end_codepoint =
        FindPercentSuffixEndCodepoint(tokens, i + 1);
    if (percent_end_codepoint != -1 &&
        ((1 << annotation_usecase) &
         options_->percentage_annotation_usecases()) != 0) {
      result->push_back(CreateAnnotatedSpan(
          new_start_codepoint, percent_end_codepoint, parsed_int_value,
          parsed_double_value, Collections::Percentage(), options_->score(),
          options_->percentage_priority_score()));
    }
  }

  return true;
}

AnnotatedSpan NumberAnnotator::CreateAnnotatedSpan(
    const int start, const int end, const int int_value,
    const double double_value, const std::string collection, const float score,
    const float priority_score) const {
  ClassificationResult classification{collection, score};
  classification.numeric_value = int_value;
  classification.numeric_double_value = double_value;
  classification.priority_score = priority_score;

  AnnotatedSpan annotated_span;
  annotated_span.span = {start, end};
  annotated_span.classification.push_back(classification);
  return annotated_span;
}

std::unordered_set<std::string>
NumberAnnotator::FromFlatbufferStringToUnordredSet(
    const flatbuffers::String* flatbuffer_percent_strings) {
  std::unordered_set<std::string> strings_set;
  if (flatbuffer_percent_strings == nullptr) {
    return strings_set;
  }

  const std::string percent_strings = flatbuffer_percent_strings->str();
  for (StringPiece suffix : strings::Split(percent_strings, '\0')) {
    std::string percent_suffix = suffix.ToString();
    percent_suffix.erase(
        std::remove_if(percent_suffix.begin(), percent_suffix.end(),
                       [](unsigned char x) { return std::isspace(x); }),
        percent_suffix.end());
    strings_set.insert(percent_suffix);
  }

  return strings_set;
}

}  // namespace libtextclassifier3
