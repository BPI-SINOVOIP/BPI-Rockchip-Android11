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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_TRANSLATE_TRANSLATE_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_TRANSLATE_TRANSLATE_H_

#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"
#include "lang_id/lang-id.h"

namespace libtextclassifier3 {

// Returns classification with "translate" when the input text is in a language
// not understood by the user.
class TranslateAnnotator {
 public:
  TranslateAnnotator(const TranslateAnnotatorOptions* options,
                     const libtextclassifier3::mobile::lang_id::LangId* langid_model,
                     const UniLib* unilib)
      : options_(options), langid_model_(langid_model), unilib_(unilib) {}

  // Returns true if a classification_result was filled with "translate"
  // classification.
  bool ClassifyText(const UnicodeText& context, CodepointSpan selection_indices,
                    const std::string& user_familiar_language_tags,
                    ClassificationResult* classification_result) const;

 protected:
  struct LanguageConfidence {
    std::string language;
    float confidence = -1.0;
  };

  // Detects language of the selection in given context using the "Backoff
  // algorithm", sorted by the score descendingly. It is based on several
  // heuristics, see the code. This is the same algorithm that TextClassifier
  // uses in Android Q.
  std::vector<LanguageConfidence> BackoffDetectLanguages(
      const UnicodeText& context, CodepointSpan selection_indices) const;

  // Returns the iterator of the next whitespace/punctuation character in given
  // text, starting from given position and going forward (iff direction == 1),
  // and backward (iff direction == -1).
  UnicodeText::const_iterator FindIndexOfNextWhitespaceOrPunctuation(
      const UnicodeText& text, int start_index, int direction) const;

  // Returns substring from given text, centered around the specified indices,
  // of certain minimum length. The substring is token aligned, so it is
  // guaranteed that the words won't be broken down.
  UnicodeText TokenAlignedSubstringAroundSpan(const UnicodeText& text,
                                              CodepointSpan indices,
                                              int minimum_length) const;

 private:
  std::string CreateSerializedEntityData(
      const std::vector<TranslateAnnotator::LanguageConfidence>& confidences)
      const;

  const TranslateAnnotatorOptions* options_;
  const libtextclassifier3::mobile::lang_id::LangId* langid_model_;
  const UniLib* unilib_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_TRANSLATE_TRANSLATE_H_
