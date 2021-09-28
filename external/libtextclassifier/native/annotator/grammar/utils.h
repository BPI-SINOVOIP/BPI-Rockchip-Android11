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

// Common utility functions for grammar annotators.

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_UTILS_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_UTILS_H_

#include "annotator/model_generated.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// Builds a tokenizer instance from options.
Tokenizer BuildTokenizer(const UniLib* unilib,
                         const GrammarTokenizerOptions* options);

// Adds a rule classification result to the |model|.
// collection: the classification entity detected.
// enabled_modes: the target to apply the given rule.
// Returns the ID associated with the created classification rule.
int AddRuleClassificationResult(const std::string& collection,
                                const ModeFlag& enabled_modes,
                                GrammarModelT* model);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_UTILS_H_
