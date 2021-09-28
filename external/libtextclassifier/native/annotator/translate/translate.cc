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

#include "annotator/translate/translate.h"

#include <memory>

#include "annotator/collections.h"
#include "annotator/entity-data_generated.h"
#include "annotator/types.h"
#include "lang_id/lang-id-wrapper.h"
#include "utils/base/logging.h"
#include "utils/i18n/locale.h"
#include "utils/utf8/unicodetext.h"
#include "lang_id/lang-id.h"

namespace libtextclassifier3 {

bool TranslateAnnotator::ClassifyText(
    const UnicodeText& context, CodepointSpan selection_indices,
    const std::string& user_familiar_language_tags,
    ClassificationResult* classification_result) const {
  std::vector<TranslateAnnotator::LanguageConfidence> confidences;
  if (options_->algorithm() ==
      TranslateAnnotatorOptions_::Algorithm::Algorithm_BACKOFF) {
    if (options_->backoff_options() == nullptr) {
      TC3_LOG(WARNING) << "No backoff options specified. Returning.";
      return false;
    }
    confidences = BackoffDetectLanguages(context, selection_indices);
  }

  if (confidences.empty()) {
    return false;
  }

  std::vector<Locale> user_familiar_languages;
  if (!ParseLocales(user_familiar_language_tags, &user_familiar_languages)) {
    TC3_LOG(WARNING) << "Couldn't parse the user-understood languages.";
    return false;
  }
  if (user_familiar_languages.empty()) {
    TC3_VLOG(INFO) << "user_familiar_languages is not set, not suggesting "
                      "translate action.";
    return false;
  }
  bool user_can_understand_language_of_text = false;
  for (const Locale& locale : user_familiar_languages) {
    if (locale.Language() == confidences[0].language) {
      user_can_understand_language_of_text = true;
      break;
    }
  }

  if (!user_can_understand_language_of_text) {
    classification_result->collection = Collections::Translate();
    classification_result->score = options_->score();
    classification_result->priority_score = options_->priority_score();
    classification_result->serialized_entity_data =
        CreateSerializedEntityData(confidences);
    return true;
  }

  return false;
}

std::string TranslateAnnotator::CreateSerializedEntityData(
    const std::vector<TranslateAnnotator::LanguageConfidence>& confidences)
    const {
  EntityDataT entity_data;
  entity_data.translate.reset(new EntityData_::TranslateT());

  for (const LanguageConfidence& confidence : confidences) {
    EntityData_::Translate_::LanguagePredictionResultT*
        language_prediction_result =
            new EntityData_::Translate_::LanguagePredictionResultT();
    language_prediction_result->language_tag = confidence.language;
    language_prediction_result->confidence_score = confidence.confidence;
    entity_data.translate->language_prediction_results.emplace_back(
        language_prediction_result);
  }
  flatbuffers::FlatBufferBuilder builder;
  FinishEntityDataBuffer(builder, EntityData::Pack(builder, &entity_data));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

std::vector<TranslateAnnotator::LanguageConfidence>
TranslateAnnotator::BackoffDetectLanguages(
    const UnicodeText& context, CodepointSpan selection_indices) const {
  const float penalize_ratio = options_->backoff_options()->penalize_ratio();
  const int min_text_size = options_->backoff_options()->min_text_size();
  if (selection_indices.second - selection_indices.first < min_text_size &&
      penalize_ratio <= 0) {
    return {};
  }

  const UnicodeText entity =
      UnicodeText::Substring(context, selection_indices.first,
                             selection_indices.second, /*do_copy=*/false);
  const std::vector<std::pair<std::string, float>> lang_id_result =
      langid::GetPredictions(langid_model_, entity.data(), entity.size_bytes());

  const float more_text_score_ratio =
      1.0f - options_->backoff_options()->subject_text_score_ratio();
  std::vector<std::pair<std::string, float>> more_lang_id_results;
  if (more_text_score_ratio >= 0) {
    const UnicodeText entity_with_context = TokenAlignedSubstringAroundSpan(
        context, selection_indices, min_text_size);
    more_lang_id_results =
        langid::GetPredictions(langid_model_, entity_with_context.data(),
                               entity_with_context.size_bytes());
  }

  const float subject_text_score_ratio =
      options_->backoff_options()->subject_text_score_ratio();

  std::map<std::string, float> result_map;
  for (const auto& [language, score] : lang_id_result) {
    result_map[language] = subject_text_score_ratio * score;
  }
  for (const auto& [language, score] : more_lang_id_results) {
    result_map[language] += more_text_score_ratio * score * penalize_ratio;
  }

  std::vector<TranslateAnnotator::LanguageConfidence> result;
  result.reserve(result_map.size());
  for (const auto& [key, value] : result_map) {
    result.push_back({key, value});
  }

  std::sort(result.begin(), result.end(),
            [](TranslateAnnotator::LanguageConfidence& a,
               TranslateAnnotator::LanguageConfidence& b) {
              return a.confidence > b.confidence;
            });
  return result;
}

UnicodeText::const_iterator
TranslateAnnotator::FindIndexOfNextWhitespaceOrPunctuation(
    const UnicodeText& text, int start_index, int direction) const {
  TC3_CHECK(direction == 1 || direction == -1);
  auto it = text.begin();
  std::advance(it, start_index);
  while (it > text.begin() && it < text.end()) {
    if (unilib_->IsWhitespace(*it) || unilib_->IsPunctuation(*it)) {
      break;
    }
    std::advance(it, direction);
  }
  return it;
}

UnicodeText TranslateAnnotator::TokenAlignedSubstringAroundSpan(
    const UnicodeText& text, CodepointSpan indices, int minimum_length) const {
  const int text_size_codepoints = text.size_codepoints();
  if (text_size_codepoints < minimum_length) {
    return UnicodeText(text, /*do_copy=*/false);
  }

  const int start = indices.first;
  const int end = indices.second;
  const int length = end - start;
  if (length >= minimum_length) {
    return UnicodeText::Substring(text, start, end, /*do_copy=*/false);
  }

  const int offset = (minimum_length - length) / 2;
  const int iter_start = std::max(
      0, std::min(start - offset, text_size_codepoints - minimum_length));
  const int iter_end =
      std::min(text_size_codepoints, iter_start + minimum_length);

  auto it_start = FindIndexOfNextWhitespaceOrPunctuation(text, iter_start, -1);
  const auto it_end = FindIndexOfNextWhitespaceOrPunctuation(text, iter_end, 1);

  // The it_start now points to whitespace/punctuation (unless it reached the
  // beginning of the string). So we'll move it one position forward to point to
  // the actual text.
  if (it_start != it_end && unilib_->IsWhitespace(*it_start)) {
    std::advance(it_start, 1);
  }

  return UnicodeText::Substring(it_start, it_end, /*do_copy=*/false);
}

}  // namespace libtextclassifier3
