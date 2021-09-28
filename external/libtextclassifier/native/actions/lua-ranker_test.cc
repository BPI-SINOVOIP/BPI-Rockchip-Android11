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

#include "actions/lua-ranker.h"

#include <string>

#include "actions/types.h"
#include "utils/flatbuffers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

MATCHER_P2(IsAction, type, response_text, "") {
  return testing::Value(arg.type, type) &&
         testing::Value(arg.response_text, response_text);
}

MATCHER_P(IsActionType, type, "") { return testing::Value(arg.type, type); }

std::string TestEntitySchema() {
  // Create fake entity data schema meta data.
  // Cannot use object oriented API here as that is not available for the
  // reflection schema.
  flatbuffers::FlatBufferBuilder schema_builder;
  std::vector<flatbuffers::Offset<reflection::Field>> fields = {
      reflection::CreateField(
          schema_builder,
          /*name=*/schema_builder.CreateString("test"),
          /*type=*/
          reflection::CreateType(schema_builder,
                                 /*base_type=*/reflection::String),
          /*id=*/0,
          /*offset=*/4)};
  std::vector<flatbuffers::Offset<reflection::Enum>> enums;
  std::vector<flatbuffers::Offset<reflection::Object>> objects = {
      reflection::CreateObject(
          schema_builder,
          /*name=*/schema_builder.CreateString("EntityData"),
          /*fields=*/
          schema_builder.CreateVectorOfSortedTables(&fields))};
  schema_builder.Finish(reflection::CreateSchema(
      schema_builder, schema_builder.CreateVectorOfSortedTables(&objects),
      schema_builder.CreateVectorOfSortedTables(&enums),
      /*(unused) file_ident=*/0,
      /*(unused) file_ext=*/0,
      /*root_table*/ objects[0]));
  return std::string(
      reinterpret_cast<const char*>(schema_builder.GetBufferPointer()),
      schema_builder.GetSize());
}

TEST(LuaRankingTest, PassThrough) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"", /*type=*/"share_location", /*score=*/0.5},
      {/*response_text=*/"", /*type=*/"add_to_collection", /*score=*/0.1}};
  const std::string test_snippet = R"(
    local result = {}
    for i=1,#actions do
      table.insert(result, i)
    end
    return result
  )";

  EXPECT_TRUE(ActionsSuggestionsLuaRanker::Create(
                  conversation, test_snippet, /*entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr, &response)
                  ->RankActions());
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsActionType("text_reply"),
                                         IsActionType("share_location"),
                                         IsActionType("add_to_collection")}));
}

TEST(LuaRankingTest, Filtering) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"", /*type=*/"share_location", /*score=*/0.5},
      {/*response_text=*/"", /*type=*/"add_to_collection", /*score=*/0.1}};
  const std::string test_snippet = R"(
    return {}
  )";

  EXPECT_TRUE(ActionsSuggestionsLuaRanker::Create(
                  conversation, test_snippet, /*entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr, &response)
                  ->RankActions());
  EXPECT_THAT(response.actions, testing::IsEmpty());
}

TEST(LuaRankingTest, Duplication) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"", /*type=*/"share_location", /*score=*/0.5},
      {/*response_text=*/"", /*type=*/"add_to_collection", /*score=*/0.1}};
  const std::string test_snippet = R"(
    local result = {}
    for i=1,#actions do
      table.insert(result, 1)
    end
    return result
  )";

  EXPECT_TRUE(ActionsSuggestionsLuaRanker::Create(
                  conversation, test_snippet, /*entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr, &response)
                  ->RankActions());
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsActionType("text_reply"),
                                         IsActionType("text_reply"),
                                         IsActionType("text_reply")}));
}

TEST(LuaRankingTest, SortByScore) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"", /*type=*/"share_location", /*score=*/0.5},
      {/*response_text=*/"", /*type=*/"add_to_collection", /*score=*/0.1}};
  const std::string test_snippet = R"(
    function testScoreSorter(a, b)
      return actions[a].score < actions[b].score
    end
    local result = {}
    for i=1,#actions do
      result[i] = i
    end
    table.sort(result, testScoreSorter)
    return result
  )";

  EXPECT_TRUE(ActionsSuggestionsLuaRanker::Create(
                  conversation, test_snippet, /*entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr, &response)
                  ->RankActions());
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsActionType("add_to_collection"),
                                         IsActionType("share_location"),
                                         IsActionType("text_reply")}));
}

TEST(LuaRankingTest, SuppressType) {
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

  EXPECT_TRUE(ActionsSuggestionsLuaRanker::Create(
                  conversation, test_snippet, /*entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr, &response)
                  ->RankActions());
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsActionType("share_location"),
                                         IsActionType("add_to_collection")}));
}

TEST(LuaRankingTest, HandlesConversation) {
  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"", /*type=*/"share_location", /*score=*/0.5},
      {/*response_text=*/"", /*type=*/"add_to_collection", /*score=*/0.1}};
  const std::string test_snippet = R"(
    local result = {}
    if messages[1].text ~= "hello hello" then
      return result
    end
    for id, action in pairs(actions) do
      if action.type ~= "text_reply" then
        table.insert(result, id)
      end
    end
    return result
  )";

  EXPECT_TRUE(ActionsSuggestionsLuaRanker::Create(
                  conversation, test_snippet, /*entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr, &response)
                  ->RankActions());
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsActionType("share_location"),
                                         IsActionType("add_to_collection")}));
}

TEST(LuaRankingTest, HandlesEntityData) {
  std::string serialized_schema = TestEntitySchema();
  const reflection::Schema* entity_data_schema =
      flatbuffers::GetRoot<reflection::Schema>(serialized_schema.data());

  // Create test entity data.
  ReflectiveFlatbufferBuilder builder(entity_data_schema);
  std::unique_ptr<ReflectiveFlatbuffer> buffer = builder.NewRoot();
  buffer->Set("test", "value_a");
  const std::string serialized_entity_data_a = buffer->Serialize();
  buffer->Set("test", "value_b");
  const std::string serialized_entity_data_b = buffer->Serialize();

  const Conversation conversation = {{{/*user_id=*/1, "hello hello"}}};
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"", /*type=*/"test",
       /*score=*/1.0, /*priority_score=*/1.0, /*annotations=*/{},
       /*serialized_entity_data=*/serialized_entity_data_a},
      {/*response_text=*/"", /*type=*/"test",
       /*score=*/1.0, /*priority_score=*/1.0, /*annotations=*/{},
       /*serialized_entity_data=*/serialized_entity_data_b},
      {/*response_text=*/"", /*type=*/"share_location", /*score=*/0.5},
      {/*response_text=*/"", /*type=*/"add_to_collection", /*score=*/0.1}};
  const std::string test_snippet = R"(
    local result = {}
    for id, action in pairs(actions) do
      if action.type == "test" and action.test == "value_a" then
        table.insert(result, id)
      end
    end
    return result
  )";

  EXPECT_TRUE(ActionsSuggestionsLuaRanker::Create(
                  conversation, test_snippet, entity_data_schema,
                  /*annotations_entity_data_schema=*/nullptr, &response)
                  ->RankActions());
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsActionType("test")}));
}

}  // namespace
}  // namespace libtextclassifier3
