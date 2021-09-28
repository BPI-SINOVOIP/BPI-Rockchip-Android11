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

#include "actions/ngram-model.h"

#include <algorithm>

#include "actions/feature-processor.h"
#include "utils/hash/farmhash.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {
namespace {

// An iterator to iterate over the initial tokens of the n-grams of a model.
class FirstTokenIterator
    : public std::iterator<std::random_access_iterator_tag,
                           /*value_type=*/uint32, /*difference_type=*/ptrdiff_t,
                           /*pointer=*/const uint32*,
                           /*reference=*/uint32&> {
 public:
  explicit FirstTokenIterator(const NGramLinearRegressionModel* model,
                              int index)
      : model_(model), index_(index) {}

  FirstTokenIterator& operator++() {
    index_++;
    return *this;
  }
  FirstTokenIterator& operator+=(ptrdiff_t dist) {
    index_ += dist;
    return *this;
  }
  ptrdiff_t operator-(const FirstTokenIterator& other_it) const {
    return index_ - other_it.index_;
  }
  uint32 operator*() const {
    const uint32 token_offset = (*model_->ngram_start_offsets())[index_];
    return (*model_->hashed_ngram_tokens())[token_offset];
  }
  int index() const { return index_; }

 private:
  const NGramLinearRegressionModel* model_;
  int index_;
};

}  // anonymous namespace

std::unique_ptr<NGramModel> NGramModel::Create(
    const UniLib* unilib, const NGramLinearRegressionModel* model,
    const Tokenizer* tokenizer) {
  if (model == nullptr) {
    return nullptr;
  }
  if (tokenizer == nullptr && model->tokenizer_options() == nullptr) {
    TC3_LOG(ERROR) << "No tokenizer options specified.";
    return nullptr;
  }
  return std::unique_ptr<NGramModel>(new NGramModel(unilib, model, tokenizer));
}

NGramModel::NGramModel(const UniLib* unilib,
                       const NGramLinearRegressionModel* model,
                       const Tokenizer* tokenizer)
    : model_(model) {
  // Create new tokenizer if options are specified, reuse feature processor
  // tokenizer otherwise.
  if (model->tokenizer_options() != nullptr) {
    owned_tokenizer_ = CreateTokenizer(model->tokenizer_options(), unilib);
    tokenizer_ = owned_tokenizer_.get();
  } else {
    tokenizer_ = tokenizer;
  }
}

// Returns whether a given n-gram matches the token stream.
bool NGramModel::IsNGramMatch(const uint32* tokens, size_t num_tokens,
                              const uint32* ngram_tokens,
                              size_t num_ngram_tokens, int max_skips) const {
  int token_idx = 0, ngram_token_idx = 0, skip_remain = 0;
  for (; token_idx < num_tokens && ngram_token_idx < num_ngram_tokens;) {
    if (tokens[token_idx] == ngram_tokens[ngram_token_idx]) {
      // Token matches. Advance both and reset the skip budget.
      ++token_idx;
      ++ngram_token_idx;
      skip_remain = max_skips;
    } else if (skip_remain > 0) {
      // No match, but we have skips left, so just advance over the token.
      ++token_idx;
      skip_remain--;
    } else {
      // No match and we're out of skips. Reject.
      return false;
    }
  }
  return ngram_token_idx == num_ngram_tokens;
}

// Calculates the total number of skip-grams that can be created for a stream
// with the given number of tokens.
uint64 NGramModel::GetNumSkipGrams(int num_tokens, int max_ngram_length,
                                   int max_skips) {
  // Start with unigrams.
  uint64 total = num_tokens;
  for (int ngram_len = 2;
       ngram_len <= max_ngram_length && ngram_len <= num_tokens; ++ngram_len) {
    // We can easily compute the expected length of the n-gram (with skips),
    // but it doesn't account for the fact that they may be longer than the
    // input and should be pruned.
    // Instead, we iterate over the distribution of effective n-gram lengths
    // and add each length individually.
    const int num_gaps = ngram_len - 1;
    const int len_min = ngram_len;
    const int len_max = ngram_len + num_gaps * max_skips;
    const int len_mid = (len_max + len_min) / 2;
    for (int len_i = len_min; len_i <= len_max; ++len_i) {
      if (len_i > num_tokens) continue;
      const int num_configs_of_len_i =
          len_i <= len_mid ? len_i - len_min + 1 : len_max - len_i + 1;
      const int num_start_offsets = num_tokens - len_i + 1;
      total += num_configs_of_len_i * num_start_offsets;
    }
  }
  return total;
}

std::pair<int, int> NGramModel::GetFirstTokenMatches(uint32 token_hash) const {
  const int num_ngrams = model_->ngram_weights()->size();
  const auto start_it = FirstTokenIterator(model_, 0);
  const auto end_it = FirstTokenIterator(model_, num_ngrams);
  const int start = std::lower_bound(start_it, end_it, token_hash).index();
  const int end = std::upper_bound(start_it, end_it, token_hash).index();
  return std::make_pair(start, end);
}

bool NGramModel::Eval(const UnicodeText& text, float* score) const {
  const std::vector<Token> raw_tokens = tokenizer_->Tokenize(text);

  // If we have no tokens, then just bail early.
  if (raw_tokens.empty()) {
    if (score != nullptr) {
      *score = model_->default_token_weight();
    }
    return false;
  }

  // Hash the tokens.
  std::vector<uint32> tokens;
  tokens.reserve(raw_tokens.size());
  for (const Token& raw_token : raw_tokens) {
    tokens.push_back(tc3farmhash::Fingerprint32(raw_token.value.data(),
                                                raw_token.value.length()));
  }

  // Calculate the total number of skip-grams that can be generated for the
  // input text.
  const uint64 num_candidates = GetNumSkipGrams(
      tokens.size(), model_->max_denom_ngram_length(), model_->max_skips());

  // For each token, see whether it denotes the start of an n-gram in the model.
  int num_matches = 0;
  float weight_matches = 0.f;
  for (size_t start_i = 0; start_i < tokens.size(); ++start_i) {
    const std::pair<int, int> ngram_range =
        GetFirstTokenMatches(tokens[start_i]);
    for (int ngram_idx = ngram_range.first; ngram_idx < ngram_range.second;
         ++ngram_idx) {
      const uint16 ngram_tokens_begin =
          (*model_->ngram_start_offsets())[ngram_idx];
      const uint16 ngram_tokens_end =
          (*model_->ngram_start_offsets())[ngram_idx + 1];
      if (IsNGramMatch(
              /*tokens=*/tokens.data() + start_i,
              /*num_tokens=*/tokens.size() - start_i,
              /*ngram_tokens=*/model_->hashed_ngram_tokens()->data() +
                  ngram_tokens_begin,
              /*num_ngram_tokens=*/ngram_tokens_end - ngram_tokens_begin,
              /*max_skips=*/model_->max_skips())) {
        ++num_matches;
        weight_matches += (*model_->ngram_weights())[ngram_idx];
      }
    }
  }

  // Calculate the score.
  const int num_misses = num_candidates - num_matches;
  const float internal_score =
      (weight_matches + (model_->default_token_weight() * num_misses)) /
      num_candidates;
  if (score != nullptr) {
    *score = internal_score;
  }
  return internal_score > model_->threshold();
}

bool NGramModel::EvalConversation(const Conversation& conversation,
                                  const int num_messages) const {
  for (int i = 1; i <= num_messages; i++) {
    const std::string& message =
        conversation.messages[conversation.messages.size() - i].text;
    const UnicodeText message_unicode(
        UTF8ToUnicodeText(message, /*do_copy=*/false));
    // Run ngram linear regression model.
    if (Eval(message_unicode)) {
      return true;
    }
  }
  return false;
}

}  // namespace libtextclassifier3
