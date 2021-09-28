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

#include "utils/base/logging.h"
#include "utils/lua-utils.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "lauxlib.h"
#include "lualib.h"
#ifdef __cplusplus
}
#endif

namespace libtextclassifier3 {
namespace {

TensorView<float> GetTensorViewForOutput(
    const TfLiteModelExecutor* model_executor,
    const tflite::Interpreter* interpreter, int output) {
  if (output < 0 || model_executor == nullptr || interpreter == nullptr) {
    return TensorView<float>::Invalid();
  }
  return model_executor->OutputView<float>(output, interpreter);
}

std::vector<std::string> GetStringTensorForOutput(
    const TfLiteModelExecutor* model_executor,
    const tflite::Interpreter* interpreter, int output) {
  if (output < 0 || model_executor == nullptr || interpreter == nullptr) {
    return {};
  }
  return model_executor->Output<std::string>(output, interpreter);
}

}  // namespace

std::unique_ptr<LuaActionsSuggestions>
LuaActionsSuggestions::CreateLuaActionsSuggestions(
    const std::string& snippet, const Conversation& conversation,
    const TfLiteModelExecutor* model_executor,
    const TensorflowLiteModelSpec* model_spec,
    const tflite::Interpreter* interpreter,
    const reflection::Schema* actions_entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema) {
  auto lua_actions =
      std::unique_ptr<LuaActionsSuggestions>(new LuaActionsSuggestions(
          snippet, conversation, model_executor, model_spec, interpreter,
          actions_entity_data_schema, annotations_entity_data_schema));
  if (!lua_actions->Initialize()) {
    TC3_LOG(ERROR)
        << "Could not initialize lua environment for actions suggestions.";
    return nullptr;
  }
  return lua_actions;
}

LuaActionsSuggestions::LuaActionsSuggestions(
    const std::string& snippet, const Conversation& conversation,
    const TfLiteModelExecutor* model_executor,
    const TensorflowLiteModelSpec* model_spec,
    const tflite::Interpreter* interpreter,
    const reflection::Schema* actions_entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema)
    : snippet_(snippet),
      conversation_(conversation),
      actions_scores_(
          model_spec == nullptr
              ? TensorView<float>::Invalid()
              : GetTensorViewForOutput(model_executor, interpreter,
                                       model_spec->output_actions_scores())),
      smart_reply_scores_(
          model_spec == nullptr
              ? TensorView<float>::Invalid()
              : GetTensorViewForOutput(model_executor, interpreter,
                                       model_spec->output_replies_scores())),
      sensitivity_score_(model_spec == nullptr
                             ? TensorView<float>::Invalid()
                             : GetTensorViewForOutput(
                                   model_executor, interpreter,
                                   model_spec->output_sensitive_topic_score())),
      triggering_score_(
          model_spec == nullptr
              ? TensorView<float>::Invalid()
              : GetTensorViewForOutput(model_executor, interpreter,
                                       model_spec->output_triggering_score())),
      smart_replies_(model_spec == nullptr ? std::vector<std::string>{}
                                           : GetStringTensorForOutput(
                                                 model_executor, interpreter,
                                                 model_spec->output_replies())),
      actions_entity_data_schema_(actions_entity_data_schema),
      annotations_entity_data_schema_(annotations_entity_data_schema) {}

bool LuaActionsSuggestions::Initialize() {
  return RunProtected([this] {
           LoadDefaultLibraries();

           // Expose conversation message stream.
           PushConversation(&conversation_.messages,
                            annotations_entity_data_schema_);
           lua_setglobal(state_, "messages");

           // Expose ML model output.
           lua_newtable(state_);

           PushTensor(&actions_scores_);
           lua_setfield(state_, /*idx=*/-2, "actions_scores");

           PushTensor(&smart_reply_scores_);
           lua_setfield(state_, /*idx=*/-2, "reply_scores");

           PushTensor(&sensitivity_score_);
           lua_setfield(state_, /*idx=*/-2, "sensitivity");

           PushTensor(&triggering_score_);
           lua_setfield(state_, /*idx=*/-2, "triggering_score");

           PushVectorIterator(&smart_replies_);
           lua_setfield(state_, /*idx=*/-2, "reply");

           lua_setglobal(state_, "model");

           return LUA_OK;
         }) == LUA_OK;
}

bool LuaActionsSuggestions::SuggestActions(
    std::vector<ActionSuggestion>* actions) {
  if (luaL_loadbuffer(state_, snippet_.data(), snippet_.size(),
                      /*name=*/nullptr) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not load actions suggestions snippet.";
    return false;
  }

  if (lua_pcall(state_, /*nargs=*/0, /*nargs=*/1, /*errfunc=*/0) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not run actions suggestions snippet.";
    return false;
  }

  if (RunProtected(
          [this, actions] {
            return ReadActions(actions_entity_data_schema_,
                               annotations_entity_data_schema_, actions);
          },
          /*num_args=*/1) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not read lua result.";
    return false;
  }
  return true;
}

}  // namespace libtextclassifier3
