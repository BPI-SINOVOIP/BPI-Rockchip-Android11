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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_EXPERIMENTAL_EXPERIMENTAL_DUMMY_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_EXPERIMENTAL_EXPERIMENTAL_DUMMY_H_

#include <string>
#include <vector>

#include "annotator/feature-processor.h"
#include "annotator/types.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

class ExperimentalAnnotator {
 public:
  // This is the dummy implementation of ExperimentalAnnotator and so it's
  // always disabled;
  static constexpr bool IsEnabled() { return false; }

  explicit ExperimentalAnnotator(const ExperimentalModel* model,
                                 const FeatureProcessor& feature_processor,
                                 const UniLib& unilib) {}

  bool Annotate(const UnicodeText& context,
                std::vector<AnnotatedSpan>* candidates) const {
    return false;
  }

  AnnotatedSpan SuggestSelection(const UnicodeText& context,
                                 CodepointSpan click) const {
    return {click, {}};
  }

  bool ClassifyText(const UnicodeText& context, CodepointSpan click,
                    ClassificationResult* result) const {
    return false;
  }
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_EXPERIMENTAL_EXPERIMENTAL_DUMMY_H_
