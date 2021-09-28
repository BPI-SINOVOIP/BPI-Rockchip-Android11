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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_CONTACT_CONTACT_ENGINE_DUMMY_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_CONTACT_CONTACT_ENGINE_DUMMY_H_

#include <string>
#include <vector>

#include "annotator/feature-processor.h"
#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// A dummy implementation of the contact engine.
class ContactEngine {
 public:
  explicit ContactEngine(const FeatureProcessor* feature_processor,
                         const UniLib* unilib,
                         const ContactAnnotatorOptions* options) {}

  bool Initialize(const std::string& serialized_config) {
    TC3_LOG(ERROR) << "No contact engine to initialize.";
    return false;
  }

  bool ClassifyText(const std::string& context, CodepointSpan selection_indices,
                    ClassificationResult* classification_result) const {
    return false;
  }

  bool Chunk(const UnicodeText& context_unicode,
             const std::vector<Token>& tokens,
             std::vector<AnnotatedSpan>* result) const {
    return true;
  }

  void AddContactMetadataToKnowledgeClassificationResult(
      ClassificationResult* classification_result) const {}
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_CONTACT_CONTACT_ENGINE_DUMMY_H_
