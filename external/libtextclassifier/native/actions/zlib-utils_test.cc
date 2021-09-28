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

#include "actions/zlib-utils.h"

#include <memory>

#include "actions/actions_model_generated.h"
#include "utils/zlib/zlib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::ElementsAre;
using testing::Field;
using testing::Pointee;

TEST(ActionsZlibUtilsTest, CompressModel) {
  ActionsModelT model;
  constexpr char kTestPattern1[] = "this is a test pattern";
  constexpr char kTestPattern2[] = "this is a second test pattern";
  constexpr char kTestOutputPattern[] = "this is an output pattern";
  model.rules.reset(new RulesModelT);
  model.rules->regex_rule.emplace_back(new RulesModel_::RegexRuleT);
  model.rules->regex_rule.back()->pattern = kTestPattern1;
  model.rules->regex_rule.emplace_back(new RulesModel_::RegexRuleT);
  model.rules->regex_rule.back()->pattern = kTestPattern2;
  model.rules->regex_rule.back()->output_pattern = kTestOutputPattern;

  // Compress the model.
  EXPECT_TRUE(CompressActionsModel(&model));

  // Sanity check that uncompressed field is removed.
  const auto is_empty_pattern =
      Pointee(Field(&libtextclassifier3::RulesModel_::RegexRuleT::pattern,
                    testing::IsEmpty()));
  EXPECT_THAT(model.rules->regex_rule,
              ElementsAre(is_empty_pattern, is_empty_pattern));
  // Pack and load the model.
  flatbuffers::FlatBufferBuilder builder;
  FinishActionsModelBuffer(builder, ActionsModel::Pack(builder, &model));
  const ActionsModel* compressed_model = GetActionsModel(
      reinterpret_cast<const char*>(builder.GetBufferPointer()));
  ASSERT_TRUE(compressed_model != nullptr);

  // Decompress the fields again and check that they match the original.
  std::unique_ptr<ZlibDecompressor> decompressor = ZlibDecompressor::Instance();
  ASSERT_TRUE(decompressor != nullptr);
  std::string uncompressed_pattern;
  EXPECT_TRUE(decompressor->MaybeDecompress(
      compressed_model->rules()->regex_rule()->Get(0)->compressed_pattern(),
      &uncompressed_pattern));
  EXPECT_EQ(uncompressed_pattern, kTestPattern1);
  EXPECT_TRUE(decompressor->MaybeDecompress(
      compressed_model->rules()->regex_rule()->Get(1)->compressed_pattern(),
      &uncompressed_pattern));
  EXPECT_EQ(uncompressed_pattern, kTestPattern2);
  EXPECT_TRUE(DecompressActionsModel(&model));
  EXPECT_EQ(model.rules->regex_rule[0]->pattern, kTestPattern1);
  EXPECT_EQ(model.rules->regex_rule[1]->pattern, kTestPattern2);
  EXPECT_EQ(model.rules->regex_rule[1]->output_pattern, kTestOutputPattern);
}

}  // namespace
}  // namespace libtextclassifier3
