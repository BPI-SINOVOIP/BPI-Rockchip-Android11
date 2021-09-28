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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_KNOWLEDGE_KNOWLEDGE_ENGINE_DUMMY_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_KNOWLEDGE_KNOWLEDGE_ENGINE_DUMMY_H_

#include <string>

#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/base/status.h"
#include "utils/optional.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// A dummy implementation of the knowledge engine.
class KnowledgeEngine {
 public:
  bool Initialize(const std::string& serialized_config, const UniLib* unilib) {
    return true;
  }

  void SetPriorityScore(float priority_score) {}

  bool ClassifyText(const std::string& text, CodepointSpan selection_indices,
                    AnnotationUsecase annotation_usecase,
                    const Optional<LocationContext>& location_context,
                    const Permissions& permissions,
                    ClassificationResult* classification_result) const {
    return false;
  }

  bool Chunk(const std::string& text, AnnotationUsecase annotation_usecase,
             const Optional<LocationContext>& location_context,
             const Permissions& permissions,
             std::vector<AnnotatedSpan>* result) const {
    return true;
  }

  Status ChunkMultipleSpans(
      const std::vector<std::string>& text_fragments,
      AnnotationUsecase annotation_usecase,
      const Optional<LocationContext>& location_context,
      const Permissions& permissions,
      std::vector<std::vector<AnnotatedSpan>>* results) const {
    return Status::OK;
  }

  bool LookUpEntity(const std::string& id,
                    std::string* serialized_knowledge_result) const {
    return false;
  }
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_KNOWLEDGE_KNOWLEDGE_ENGINE_DUMMY_H_
