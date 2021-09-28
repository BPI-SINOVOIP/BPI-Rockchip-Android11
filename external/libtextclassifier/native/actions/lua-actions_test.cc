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

#include "actions/lua-actions.h"

#include <map>
#include <string>

#include "actions/test-utils.h"
#include "actions/types.h"
#include "utils/tflite-model-executor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::ElementsAre;

TEST(LuaActions, SimpleAction) {
  Conversation conversation;
  const std::string test_snippet = R"(
    return {{ type = "test_action" }}
  )";
  std::vector<ActionSuggestion> actions;
  EXPECT_TRUE(LuaActionsSuggestions::CreateLuaActionsSuggestions(
                  test_snippet, conversation,
                  /*model_executor=*/nullptr,
                  /*model_spec=*/nullptr,
                  /*interpreter=*/nullptr,
                  /*actions_entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr)
                  ->SuggestActions(&actions));
  EXPECT_THAT(actions, ElementsAre(IsActionOfType("test_action")));
}

TEST(LuaActions, ConversationActions) {
  Conversation conversation;
  conversation.messages.push_back({/*user_id=*/0, "hello there!"});
  conversation.messages.push_back({/*user_id=*/1, "general kenobi!"});
  const std::string test_snippet = R"(
    local actions = {}
    for i, message in pairs(messages) do
      if i < #messages then
        if message.text == "hello there!" and
           messages[i+1].text == "general kenobi!" then
           table.insert(actions, {
             type = "text_reply",
             response_text = "you are a bold one!"
           })
        end
        if message.text == "i am the senate!" and
           messages[i+1].text == "not yet!" then
           table.insert(actions, {
             type = "text_reply",
             response_text = "it's treason then"
           })
        end
      end
    end
    return actions;
  )";
  std::vector<ActionSuggestion> actions;
  EXPECT_TRUE(LuaActionsSuggestions::CreateLuaActionsSuggestions(
                  test_snippet, conversation,
                  /*model_executor=*/nullptr,
                  /*model_spec=*/nullptr,
                  /*interpreter=*/nullptr,
                  /*actions_entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr)
                  ->SuggestActions(&actions));
  EXPECT_THAT(actions, ElementsAre(IsSmartReply("you are a bold one!")));
}

TEST(LuaActions, SimpleModelAction) {
  Conversation conversation;
  const std::string test_snippet = R"(
    if #model.actions_scores == 0 then
      return {{ type = "test_action" }}
    end
    return {}
  )";
  std::vector<ActionSuggestion> actions;
  EXPECT_TRUE(LuaActionsSuggestions::CreateLuaActionsSuggestions(
                  test_snippet, conversation,
                  /*model_executor=*/nullptr,
                  /*model_spec=*/nullptr,
                  /*interpreter=*/nullptr,
                  /*actions_entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr)
                  ->SuggestActions(&actions));
  EXPECT_THAT(actions, ElementsAre(IsActionOfType("test_action")));
}

TEST(LuaActions, SimpleModelRepliesAction) {
  Conversation conversation;
  const std::string test_snippet = R"(
    if #model.reply == 0 then
      return {{ type = "test_action" }}
    end
    return {}
  )";
  std::vector<ActionSuggestion> actions;
  EXPECT_TRUE(LuaActionsSuggestions::CreateLuaActionsSuggestions(
                  test_snippet, conversation,
                  /*model_executor=*/nullptr,
                  /*model_spec=*/nullptr,
                  /*interpreter=*/nullptr,
                  /*actions_entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr)
                  ->SuggestActions(&actions));
  EXPECT_THAT(actions, ElementsAre(IsActionOfType("test_action")));
}

TEST(LuaActions, AnnotationActions) {
  AnnotatedSpan annotation;
  annotation.span = {11, 15};
  annotation.classification = {ClassificationResult("address", 1.0)};
  Conversation conversation = {{{/*user_id=*/1, "are you at home?",
                                 /*reference_time_ms_utc=*/0,
                                 /*reference_timezone=*/"Europe/Zurich",
                                 /*annotations=*/{annotation},
                                 /*locales=*/"en"}}};
  const std::string test_snippet = R"(
    local actions = {}
    local last_message = messages[#messages]
    for i, annotation in pairs(last_message.annotation) do
      if #annotation.classification > 0 then
        if annotation.classification[1].collection == "address" then
           local text = string.sub(last_message.text,
                            annotation.span["begin"] + 1,
                            annotation.span["end"])
           table.insert(actions, {
             type = "text_reply",
             response_text = "i am at " .. text,
             annotation = {{
               name = "location",
               span = {
                 text = text
               },
               entity = annotation.classification[1]
             }},
           })
        end
      end
    end
    return actions;
  )";
  std::vector<ActionSuggestion> actions;
  EXPECT_TRUE(LuaActionsSuggestions::CreateLuaActionsSuggestions(
                  test_snippet, conversation,
                  /*model_executor=*/nullptr,
                  /*model_spec=*/nullptr,
                  /*interpreter=*/nullptr,
                  /*actions_entity_data_schema=*/nullptr,
                  /*annotations_entity_data_schema=*/nullptr)
                  ->SuggestActions(&actions));
  EXPECT_THAT(actions, ElementsAre(IsSmartReply("i am at home")));
  EXPECT_EQ("address", actions[0].annotations[0].entity.collection);
}

TEST(LuaActions, EntityData) {
  std::string test_schema = TestEntityDataSchema();
  Conversation conversation = {{{/*user_id=*/1, "hello there"}}};
  const std::string test_snippet = R"(
    return {{
      type = "test",
      entity = {
        greeting = "hello",
        location = "there",
        person = "Kenobi",
      },
    }};
  )";
  std::vector<ActionSuggestion> actions;
  EXPECT_TRUE(LuaActionsSuggestions::CreateLuaActionsSuggestions(
                  test_snippet, conversation,
                  /*model_executor=*/nullptr,
                  /*model_spec=*/nullptr,
                  /*interpreter=*/nullptr,
                  /*actions_entity_data_schema=*/
                  flatbuffers::GetRoot<reflection::Schema>(test_schema.data()),
                  /*annotations_entity_data_schema=*/nullptr)
                  ->SuggestActions(&actions));
  EXPECT_THAT(actions, testing::SizeIs(1));
  EXPECT_EQ("test", actions.front().type);
  const flatbuffers::Table* entity =
      flatbuffers::GetAnyRoot(reinterpret_cast<const unsigned char*>(
          actions.front().serialized_entity_data.data()));
  EXPECT_EQ(entity->GetPointer<const flatbuffers::String*>(/*field=*/4)->str(),
            "hello");
  EXPECT_EQ(entity->GetPointer<const flatbuffers::String*>(/*field=*/6)->str(),
            "there");
  EXPECT_EQ(entity->GetPointer<const flatbuffers::String*>(/*field=*/8)->str(),
            "Kenobi");
}

}  // namespace
}  // namespace libtextclassifier3
