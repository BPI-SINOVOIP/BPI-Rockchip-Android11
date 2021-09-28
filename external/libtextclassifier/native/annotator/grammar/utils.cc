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

#include "annotator/grammar/utils.h"

#include "utils/grammar/utils/rules.h"

namespace libtextclassifier3 {
namespace {

using ::libtextclassifier3::GrammarModel_::RuleClassificationResultT;

}  // namespace

Tokenizer BuildTokenizer(const UniLib* unilib,
                         const GrammarTokenizerOptions* options) {
  TC3_CHECK(options != nullptr);

  std::vector<const TokenizationCodepointRange*> codepoint_config;
  if (options->tokenization_codepoint_config() != nullptr) {
    codepoint_config.insert(codepoint_config.end(),
                            options->tokenization_codepoint_config()->begin(),
                            options->tokenization_codepoint_config()->end());
  }
  std::vector<const CodepointRange*> internal_codepoint_config;
  if (options->internal_tokenizer_codepoint_ranges() != nullptr) {
    internal_codepoint_config.insert(
        internal_codepoint_config.end(),
        options->internal_tokenizer_codepoint_ranges()->begin(),
        options->internal_tokenizer_codepoint_ranges()->end());
  }

  const bool tokenize_on_script_change =
      options->tokenization_codepoint_config() != nullptr &&
      options->tokenize_on_script_change();
  return Tokenizer(options->tokenization_type(), unilib, codepoint_config,
                   internal_codepoint_config, tokenize_on_script_change,
                   /*icu_preserve_whitespace_tokens=*/false);
}

int AddRuleClassificationResult(const std::string& collection,
                                const ModeFlag& enabled_modes,
                                GrammarModelT* model) {
  const int result_id = model->rule_classification_result.size();
  model->rule_classification_result.emplace_back(new RuleClassificationResultT);
  RuleClassificationResultT* result =
      model->rule_classification_result.back().get();
  result->collection_name = collection;
  result->enabled_modes = enabled_modes;
  return result_id;
}

}  // namespace libtextclassifier3
