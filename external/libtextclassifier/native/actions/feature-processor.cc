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

#include "actions/feature-processor.h"

namespace libtextclassifier3 {
namespace {
TokenFeatureExtractorOptions BuildTokenFeatureExtractorOptions(
    const ActionsTokenFeatureProcessorOptions* const options) {
  TokenFeatureExtractorOptions extractor_options;
  extractor_options.num_buckets = options->num_buckets();
  if (options->chargram_orders() != nullptr) {
    for (int order : *options->chargram_orders()) {
      extractor_options.chargram_orders.push_back(order);
    }
  }
  extractor_options.max_word_length = options->max_token_length();
  extractor_options.extract_case_feature = options->extract_case_feature();
  extractor_options.unicode_aware_features = options->unicode_aware_features();
  extractor_options.extract_selection_mask_feature = false;
  if (options->regexp_features() != nullptr) {
    for (const auto regexp_feature : *options->regexp_features()) {
      extractor_options.regexp_features.push_back(regexp_feature->str());
    }
  }
  extractor_options.remap_digits = options->remap_digits();
  extractor_options.lowercase_tokens = options->lowercase_tokens();
  return extractor_options;
}
}  // namespace

std::unique_ptr<Tokenizer> CreateTokenizer(
    const ActionsTokenizerOptions* options, const UniLib* unilib) {
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
  return std::unique_ptr<Tokenizer>(new Tokenizer(
      options->type(), unilib, codepoint_config, internal_codepoint_config,
      tokenize_on_script_change, options->icu_preserve_whitespace_tokens()));
}

ActionsFeatureProcessor::ActionsFeatureProcessor(
    const ActionsTokenFeatureProcessorOptions* options, const UniLib* unilib)
    : options_(options),
      tokenizer_(CreateTokenizer(options->tokenizer_options(), unilib)),
      token_feature_extractor_(BuildTokenFeatureExtractorOptions(options),
                               unilib) {}

int ActionsFeatureProcessor::GetTokenEmbeddingSize() const {
  return options_->embedding_size() +
         token_feature_extractor_.DenseFeaturesCount();
}

bool ActionsFeatureProcessor::AppendFeatures(
    const std::vector<int>& sparse_features,
    const std::vector<float>& dense_features,
    const EmbeddingExecutor* embedding_executor,
    std::vector<float>* output_features) const {
  // Embed the sparse features, appending them directly to the output.
  const int embedding_size = options_->embedding_size();
  output_features->resize(output_features->size() + embedding_size);
  float* output_features_end =
      output_features->data() + output_features->size();
  if (!embedding_executor->AddEmbedding(
          TensorView<int>(sparse_features.data(),
                          {static_cast<int>(sparse_features.size())}),
          /*dest=*/output_features_end - embedding_size,
          /*dest_size=*/embedding_size)) {
    TC3_LOG(ERROR) << "Could not embed token's sparse features.";
    return false;
  }

  // Append the dense features to the output.
  output_features->insert(output_features->end(), dense_features.begin(),
                          dense_features.end());
  return true;
}

bool ActionsFeatureProcessor::AppendTokenFeatures(
    const Token& token, const EmbeddingExecutor* embedding_executor,
    std::vector<float>* output_features) const {
  // Extract the sparse and dense features.
  std::vector<int> sparse_features;
  std::vector<float> dense_features;
  if (!token_feature_extractor_.Extract(token, /*(unused) is_in_span=*/false,
                                        &sparse_features, &dense_features)) {
    TC3_LOG(ERROR) << "Could not extract token's features.";
    return false;
  }
  return AppendFeatures(sparse_features, dense_features, embedding_executor,
                        output_features);
}

bool ActionsFeatureProcessor::AppendTokenFeatures(
    const std::vector<Token>& tokens,
    const EmbeddingExecutor* embedding_executor,
    std::vector<float>* output_features) const {
  for (const Token& token : tokens) {
    if (!AppendTokenFeatures(token, embedding_executor, output_features)) {
      return false;
    }
  }
  return true;
}

}  // namespace libtextclassifier3
