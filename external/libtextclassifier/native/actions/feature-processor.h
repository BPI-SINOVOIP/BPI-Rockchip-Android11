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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_FEATURE_PROCESSOR_H_
#define LIBTEXTCLASSIFIER_ACTIONS_FEATURE_PROCESSOR_H_

#include <memory>

#include "actions/actions_model_generated.h"
#include "annotator/model-executor.h"
#include "annotator/types.h"
#include "utils/token-feature-extractor.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// Create tokenizer from options.
std::unique_ptr<Tokenizer> CreateTokenizer(
    const ActionsTokenizerOptions* options, const UniLib* unilib);

// Feature processor for the actions suggestions model.
class ActionsFeatureProcessor {
 public:
  explicit ActionsFeatureProcessor(
      const ActionsTokenFeatureProcessorOptions* options, const UniLib* unilib);

  // Embeds and appends features to the output vector.
  bool AppendFeatures(const std::vector<int>& sparse_features,
                      const std::vector<float>& dense_features,
                      const EmbeddingExecutor* embedding_executor,
                      std::vector<float>* output_features) const;

  // Extracts the features of a token and appends them to the output vector.
  bool AppendTokenFeatures(const Token& token,
                           const EmbeddingExecutor* embedding_executor,
                           std::vector<float>* output_features) const;

  // Extracts the features of a vector of tokens and appends each to the output
  // vector.
  bool AppendTokenFeatures(const std::vector<Token>& tokens,
                           const EmbeddingExecutor* embedding_executor,
                           std::vector<float>* output_features) const;

  int GetTokenEmbeddingSize() const;

  const Tokenizer* tokenizer() const { return tokenizer_.get(); }

 private:
  const ActionsTokenFeatureProcessorOptions* options_;
  const std::unique_ptr<Tokenizer> tokenizer_;
  const TokenFeatureExtractor token_feature_extractor_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_FEATURE_PROCESSOR_H_
