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

#include "annotator/zlib-utils.h"

#include <memory>

#include "annotator/model_generated.h"
#include "utils/zlib/zlib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {

TEST(AnnotatorZlibUtilsTest, CompressModel) {
  ModelT model;
  model.regex_model.reset(new RegexModelT);
  model.regex_model->patterns.emplace_back(new RegexModel_::PatternT);
  model.regex_model->patterns.back()->pattern = "this is a test pattern";
  model.regex_model->patterns.emplace_back(new RegexModel_::PatternT);
  model.regex_model->patterns.back()->pattern = "this is a second test pattern";

  model.datetime_model.reset(new DatetimeModelT);
  model.datetime_model->patterns.emplace_back(new DatetimeModelPatternT);
  model.datetime_model->patterns.back()->regexes.emplace_back(
      new DatetimeModelPattern_::RegexT);
  model.datetime_model->patterns.back()->regexes.back()->pattern =
      "an example datetime pattern";
  model.datetime_model->extractors.emplace_back(new DatetimeModelExtractorT);
  model.datetime_model->extractors.back()->pattern =
      "an example datetime extractor";

  model.intent_options.reset(new IntentFactoryModelT);
  model.intent_options->generator.emplace_back(
      new IntentFactoryModel_::IntentGeneratorT);
  const std::string intent_generator1 = "lua generator 1";
  model.intent_options->generator.back()->lua_template_generator =
      std::vector<uint8_t>(intent_generator1.begin(), intent_generator1.end());
  model.intent_options->generator.emplace_back(
      new IntentFactoryModel_::IntentGeneratorT);
  const std::string intent_generator2 = "lua generator 2";
  model.intent_options->generator.back()->lua_template_generator =
      std::vector<uint8_t>(intent_generator2.begin(), intent_generator2.end());

  // NOTE: The resource strings contain some repetition, so that the compressed
  // version is smaller than the uncompressed one. Because the compression code
  // looks at that as well.
  model.resources.reset(new ResourcePoolT);
  model.resources->resource_entry.emplace_back(new ResourceEntryT);
  model.resources->resource_entry.back()->resource.emplace_back(new ResourceT);
  model.resources->resource_entry.back()->resource.back()->content =
      "rrrrrrrrrrrrr1.1";
  model.resources->resource_entry.back()->resource.emplace_back(new ResourceT);
  model.resources->resource_entry.back()->resource.back()->content =
      "rrrrrrrrrrrrr1.2";
  model.resources->resource_entry.emplace_back(new ResourceEntryT);
  model.resources->resource_entry.back()->resource.emplace_back(new ResourceT);
  model.resources->resource_entry.back()->resource.back()->content =
      "rrrrrrrrrrrrr2.1";
  model.resources->resource_entry.back()->resource.emplace_back(new ResourceT);
  model.resources->resource_entry.back()->resource.back()->content =
      "rrrrrrrrrrrrr2.2";

  // Compress the model.
  EXPECT_TRUE(CompressModel(&model));

  // Sanity check that uncompressed field is removed.
  EXPECT_TRUE(model.regex_model->patterns[0]->pattern.empty());
  EXPECT_TRUE(model.regex_model->patterns[1]->pattern.empty());
  EXPECT_TRUE(model.datetime_model->patterns[0]->regexes[0]->pattern.empty());
  EXPECT_TRUE(model.datetime_model->extractors[0]->pattern.empty());
  EXPECT_TRUE(
      model.intent_options->generator[0]->lua_template_generator.empty());
  EXPECT_TRUE(
      model.intent_options->generator[1]->lua_template_generator.empty());
  EXPECT_TRUE(model.resources->resource_entry[0]->resource[0]->content.empty());
  EXPECT_TRUE(model.resources->resource_entry[0]->resource[1]->content.empty());
  EXPECT_TRUE(model.resources->resource_entry[1]->resource[0]->content.empty());
  EXPECT_TRUE(model.resources->resource_entry[1]->resource[1]->content.empty());

  // Pack and load the model.
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(Model::Pack(builder, &model));
  const Model* compressed_model =
      GetModel(reinterpret_cast<const char*>(builder.GetBufferPointer()));
  ASSERT_TRUE(compressed_model != nullptr);

  // Decompress the fields again and check that they match the original.
  std::unique_ptr<ZlibDecompressor> decompressor = ZlibDecompressor::Instance();
  ASSERT_TRUE(decompressor != nullptr);
  std::string uncompressed_pattern;
  EXPECT_TRUE(decompressor->MaybeDecompress(
      compressed_model->regex_model()->patterns()->Get(0)->compressed_pattern(),
      &uncompressed_pattern));
  EXPECT_EQ(uncompressed_pattern, "this is a test pattern");
  EXPECT_TRUE(decompressor->MaybeDecompress(
      compressed_model->regex_model()->patterns()->Get(1)->compressed_pattern(),
      &uncompressed_pattern));
  EXPECT_EQ(uncompressed_pattern, "this is a second test pattern");
  EXPECT_TRUE(decompressor->MaybeDecompress(compressed_model->datetime_model()
                                                ->patterns()
                                                ->Get(0)
                                                ->regexes()
                                                ->Get(0)
                                                ->compressed_pattern(),
                                            &uncompressed_pattern));
  EXPECT_EQ(uncompressed_pattern, "an example datetime pattern");
  EXPECT_TRUE(decompressor->MaybeDecompress(compressed_model->datetime_model()
                                                ->extractors()
                                                ->Get(0)
                                                ->compressed_pattern(),
                                            &uncompressed_pattern));
  EXPECT_EQ(uncompressed_pattern, "an example datetime extractor");

  EXPECT_TRUE(DecompressModel(&model));
  EXPECT_EQ(model.regex_model->patterns[0]->pattern, "this is a test pattern");
  EXPECT_EQ(model.regex_model->patterns[1]->pattern,
            "this is a second test pattern");
  EXPECT_EQ(model.datetime_model->patterns[0]->regexes[0]->pattern,
            "an example datetime pattern");
  EXPECT_EQ(model.datetime_model->extractors[0]->pattern,
            "an example datetime extractor");
  EXPECT_EQ(
      model.intent_options->generator[0]->lua_template_generator,
      std::vector<uint8_t>(intent_generator1.begin(), intent_generator1.end()));
  EXPECT_EQ(
      model.intent_options->generator[1]->lua_template_generator,
      std::vector<uint8_t>(intent_generator2.begin(), intent_generator2.end()));
  EXPECT_EQ(model.resources->resource_entry[0]->resource[0]->content,
            "rrrrrrrrrrrrr1.1");
  EXPECT_EQ(model.resources->resource_entry[0]->resource[1]->content,
            "rrrrrrrrrrrrr1.2");
  EXPECT_EQ(model.resources->resource_entry[1]->resource[0]->content,
            "rrrrrrrrrrrrr2.1");
  EXPECT_EQ(model.resources->resource_entry[1]->resource[1]->content,
            "rrrrrrrrrrrrr2.2");
}

}  // namespace libtextclassifier3
