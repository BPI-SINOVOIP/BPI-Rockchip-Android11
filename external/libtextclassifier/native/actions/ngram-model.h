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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_NGRAM_MODEL_H_
#define LIBTEXTCLASSIFIER_ACTIONS_NGRAM_MODEL_H_

#include <memory>

#include "actions/actions_model_generated.h"
#include "actions/types.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

class NGramModel {
 public:
  static std::unique_ptr<NGramModel> Create(
      const UniLib* unilib, const NGramLinearRegressionModel* model,
      const Tokenizer* tokenizer);

  // Evaluates an n-gram linear regression model, and tests against the
  // threshold. Returns true in case of a positive classification. The caller
  // may also optionally query the score.
  bool Eval(const UnicodeText& text, float* score = nullptr) const;

  // Evaluates an n-gram linear regression model against all messages in a
  // conversation and returns true in case of any positive classification.
  bool EvalConversation(const Conversation& conversation,
                        const int num_messages) const;

  // Exposed for testing only.
  static uint64 GetNumSkipGrams(int num_tokens, int max_ngram_length,
                                int max_skips);

 private:
  NGramModel(const UniLib* unilib, const NGramLinearRegressionModel* model,
             const Tokenizer* tokenizer);

  // Returns the (begin,end] range of n-grams where the first hashed token
  // matches the given value.
  std::pair<int, int> GetFirstTokenMatches(uint32 token_hash) const;

  // Returns whether a given n-gram matches the token stream.
  bool IsNGramMatch(const uint32* tokens, size_t num_tokens,
                    const uint32* ngram_tokens, size_t num_ngram_tokens,
                    int max_skips) const;

  const NGramLinearRegressionModel* model_;
  const Tokenizer* tokenizer_;
  std::unique_ptr<Tokenizer> owned_tokenizer_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_NGRAM_MODEL_H_
