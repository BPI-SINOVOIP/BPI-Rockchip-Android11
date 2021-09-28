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

#include "actions/ranker.h"

#include <string>

#include "actions/types.h"
#include "utils/zlib/zlib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

MATCHER_P3(IsAction, type, response_text, score, "") {
  return testing::Value(arg.type, type) &&
         testing::Value(arg.response_text, response_text) &&
         testing::Value(arg.score, score);
}

MATCHER_P(IsActionType, type, "") { return testing::Value(arg.type, type); }

TEST(RankingTest, DeduplicationSmartReply) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"hello there", /*type=*/"text_reply", /*score=*/0.5}};

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);
  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray({IsAction("text_reply", "hello there", 1.0)}));
}

TEST(RankingTest, DeduplicationExtraData) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0, /*priority_score=*/0.0},
      {/*response_text=*/"hello there", /*type=*/"text_reply", /*score=*/0.5,
       /*priority_score=*/0.0},
      {/*response_text=*/"hello there", /*type=*/"text_reply", /*score=*/0.6,
       /*priority_score=*/0.0,
       /*annotations=*/{}, /*serialized_entity_data=*/"test"},
  };

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);
  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray({IsAction("text_reply", "hello there", 1.0),
                                 // Is kept as it has different entity data.
                                 IsAction("text_reply", "hello there", 0.6)}));
}

TEST(RankingTest, DeduplicationAnnotations) {
  const Conversation conversation = {
      {{/*user_id=*/1, "742 Evergreen Terrace, the number is 1-800-TESTING"}}};
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/0.5,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/1.0,
                                /*priority_score=*/2.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{37, 50},
                       /*text=*/"1-800-TESTING"};
    annotation.entity = ClassificationResult("phone", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/0.5,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsAction("view_map", "", 1.0),
                                         IsAction("call_phone", "", 0.5)}));
}

TEST(RankingTest, DeduplicationAnnotationsByPriorityScore) {
  const Conversation conversation = {
      {{/*user_id=*/1, "742 Evergreen Terrace, the number is 1-800-TESTING"}}};
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/0.6,
                                /*priority_score=*/2.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/1.0,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{37, 50},
                       /*text=*/"1-800-TESTING"};
    annotation.entity = ClassificationResult("phone", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/0.5,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);
  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray(
          {IsAction("view_map", "",
                    0.6),  // lower score wins, as priority score is higher
           IsAction("call_phone", "", 0.5)}));
}

TEST(RankingTest, DeduplicatesConflictingActions) {
  const Conversation conversation = {{{/*user_id=*/1, "code A-911"}}};
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{7, 10},
                       /*text=*/"911"};
    annotation.entity = ClassificationResult("phone", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/1.0,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{5, 10},
                       /*text=*/"A-911"};
    annotation.entity = ClassificationResult("code", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"copy_code",
                                /*score=*/1.0,
                                /*priority_score=*/2.0,
                                /*annotations=*/{annotation}});
  }
  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsAction("copy_code", "", 1.0)}));
}

TEST(RankingTest, HandlesCompressedLuaScript) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"", /*type=*/"share_location", /*score=*/0.5},
      {/*response_text=*/"", /*type=*/"add_to_collection", /*score=*/0.1}};
  const std::string test_snippet = R"(
    local result = {}
    for id, action in pairs(actions) do
      if action.type ~= "text_reply" then
        table.insert(result, id)
      end
    end
    return result
  )";
  RankingOptionsT options;
  options.compressed_lua_ranking_script.reset(new CompressedBufferT);
  std::unique_ptr<ZlibCompressor> compressor = ZlibCompressor::Instance();
  compressor->Compress(test_snippet,
                       options.compressed_lua_ranking_script.get());
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));

  std::unique_ptr<ZlibDecompressor> decompressor = ZlibDecompressor::Instance();
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      decompressor.get(), /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsActionType("share_location"),
                                         IsActionType("add_to_collection")}));
}

TEST(RankingTest, SuppressSmartRepliesWithAction) {
  const Conversation conversation = {{{/*user_id=*/1, "should i call 911"}}};
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{5, 8},
                       /*text=*/"911"};
    annotation.entity = ClassificationResult("phone", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/1.0,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }
  response.actions.push_back({/*response_text=*/"How are you?",
                              /*type=*/"text_reply"});
  RankingOptionsT options;
  options.suppress_smart_replies_with_actions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);

  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsAction("call_phone", "", 1.0)}));
}

TEST(RankingTest, GroupsActionsByAnnotations) {
  const Conversation conversation = {{{/*user_id=*/1, "should i call 911"}}};
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{5, 8},
                       /*text=*/"911"};
    annotation.entity = ClassificationResult("phone", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/1.0,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"add_contact",
                                /*score=*/0.0,
                                /*priority_score=*/0.0,
                                /*annotations=*/{annotation}});
  }
  response.actions.push_back({/*response_text=*/"How are you?",
                              /*type=*/"text_reply",
                              /*score=*/0.5});
  RankingOptionsT options;
  options.group_by_annotations = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);

  // The text reply should be last, even though it has a higher score than the
  // `add_contact` action.
  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray({IsAction("call_phone", "", 1.0),
                                 IsAction("add_contact", "", 0.0),
                                 IsAction("text_reply", "How are you?", 0.5)}));
}

TEST(RankingTest, SortsActionsByScore) {
  const Conversation conversation = {{{/*user_id=*/1, "should i call 911"}}};
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{5, 8},
                       /*text=*/"911"};
    annotation.entity = ClassificationResult("phone", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/1.0,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"add_contact",
                                /*score=*/0.0,
                                /*priority_score=*/0.0,
                                /*annotations=*/{annotation}});
  }
  response.actions.push_back({/*response_text=*/"How are you?",
                              /*type=*/"text_reply",
                              /*score=*/0.5});
  RankingOptionsT options;
  // Don't group by annotation.
  options.group_by_annotations = false;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()),
      /*decompressor=*/nullptr, /*smart_reply_action_type=*/"text_reply");

  ranker->RankActions(conversation, &response);

  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray({IsAction("call_phone", "", 1.0),
                                 IsAction("text_reply", "How are you?", 0.5),
                                 IsAction("add_contact", "", 0.0)}));
}

}  // namespace
}  // namespace libtextclassifier3
