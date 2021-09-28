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

#include "actions/actions_model_generated.h"
#include "annotator/model-executor.h"
#include "utils/tensor-view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using ::testing::FloatEq;
using ::testing::SizeIs;

// EmbeddingExecutor that always returns features based on
// the id of the sparse features.
class FakeEmbeddingExecutor : public EmbeddingExecutor {
 public:
  bool AddEmbedding(const TensorView<int>& sparse_features, float* dest,
                    const int dest_size) const override {
    TC3_CHECK_GE(dest_size, 4);
    EXPECT_THAT(sparse_features, SizeIs(1));
    dest[0] = sparse_features.data()[0];
    dest[1] = sparse_features.data()[0];
    dest[2] = -sparse_features.data()[0];
    dest[3] = -sparse_features.data()[0];
    return true;
  }

 private:
  std::vector<float> storage_;
};

class FeatureProcessorTest : public ::testing::Test {
 protected:
  FeatureProcessorTest() : INIT_UNILIB_FOR_TESTING(unilib_) {}

  flatbuffers::DetachedBuffer PackFeatureProcessorOptions(
      ActionsTokenFeatureProcessorOptionsT* options) const {
    flatbuffers::FlatBufferBuilder builder;
    builder.Finish(CreateActionsTokenFeatureProcessorOptions(builder, options));
    return builder.Release();
  }

  FakeEmbeddingExecutor embedding_executor_;
  UniLib unilib_;
};

TEST_F(FeatureProcessorTest, TokenEmbeddings) {
  ActionsTokenFeatureProcessorOptionsT options;
  options.embedding_size = 4;
  options.tokenizer_options.reset(new ActionsTokenizerOptionsT);

  flatbuffers::DetachedBuffer options_fb =
      PackFeatureProcessorOptions(&options);
  ActionsFeatureProcessor feature_processor(
      flatbuffers::GetRoot<ActionsTokenFeatureProcessorOptions>(
          options_fb.data()),
      &unilib_);

  Token token("aaa", 0, 3);
  std::vector<float> token_features;
  EXPECT_TRUE(feature_processor.AppendTokenFeatures(token, &embedding_executor_,
                                                    &token_features));
  EXPECT_THAT(token_features, SizeIs(4));
}

TEST_F(FeatureProcessorTest, TokenEmbeddingsCaseFeature) {
  ActionsTokenFeatureProcessorOptionsT options;
  options.embedding_size = 4;
  options.extract_case_feature = true;
  options.tokenizer_options.reset(new ActionsTokenizerOptionsT);

  flatbuffers::DetachedBuffer options_fb =
      PackFeatureProcessorOptions(&options);
  ActionsFeatureProcessor feature_processor(
      flatbuffers::GetRoot<ActionsTokenFeatureProcessorOptions>(
          options_fb.data()),
      &unilib_);

  Token token("Aaa", 0, 3);
  std::vector<float> token_features;
  EXPECT_TRUE(feature_processor.AppendTokenFeatures(token, &embedding_executor_,
                                                    &token_features));
  EXPECT_THAT(token_features, SizeIs(5));
  EXPECT_THAT(token_features[4], FloatEq(1.0));
}

TEST_F(FeatureProcessorTest, MultipleTokenEmbeddingsCaseFeature) {
  ActionsTokenFeatureProcessorOptionsT options;
  options.embedding_size = 4;
  options.extract_case_feature = true;
  options.tokenizer_options.reset(new ActionsTokenizerOptionsT);

  flatbuffers::DetachedBuffer options_fb =
      PackFeatureProcessorOptions(&options);
  ActionsFeatureProcessor feature_processor(
      flatbuffers::GetRoot<ActionsTokenFeatureProcessorOptions>(
          options_fb.data()),
      &unilib_);

  const std::vector<Token> tokens = {Token("Aaa", 0, 3), Token("bbb", 4, 7),
                                     Token("Cccc", 8, 12)};
  std::vector<float> token_features;
  EXPECT_TRUE(feature_processor.AppendTokenFeatures(
      tokens, &embedding_executor_, &token_features));
  EXPECT_THAT(token_features, SizeIs(15));
  EXPECT_THAT(token_features[4], FloatEq(1.0));
  EXPECT_THAT(token_features[9], FloatEq(-1.0));
  EXPECT_THAT(token_features[14], FloatEq(1.0));
}

}  // namespace
}  // namespace libtextclassifier3
