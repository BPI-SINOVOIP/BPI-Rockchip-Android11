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

std::unique_ptr<ActionsSuggestionsLuaRanker>
ActionsSuggestionsLuaRanker::Create(
    const Conversation& conversation, const std::string& ranker_code,
    const reflection::Schema* entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema,
    ActionsSuggestionsResponse* response) {
  auto ranker = std::unique_ptr<ActionsSuggestionsLuaRanker>(
      new ActionsSuggestionsLuaRanker(
          conversation, ranker_code, entity_data_schema,
          annotations_entity_data_schema, response));
  if (!ranker->Initialize()) {
    TC3_LOG(ERROR) << "Could not initialize lua environment for ranker.";
    return nullptr;
  }
  return ranker;
}

bool ActionsSuggestionsLuaRanker::Initialize() {
  return RunProtected([this] {
           LoadDefaultLibraries();

           // Expose generated actions.
           PushActions(&response_->actions, actions_entity_data_schema_,
                       annotations_entity_data_schema_);
           lua_setglobal(state_, "actions");

           // Expose conversation message stream.
           PushConversation(&conversation_.messages,
                            annotations_entity_data_schema_);
           lua_setglobal(state_, "messages");
           return LUA_OK;
         }) == LUA_OK;
}

int ActionsSuggestionsLuaRanker::ReadActionsRanking() {
  if (lua_type(state_, /*idx=*/-1) != LUA_TTABLE) {
    TC3_LOG(ERROR) << "Expected actions table, got: "
                   << lua_type(state_, /*idx=*/-1);
    lua_pop(state_, 1);
    lua_error(state_);
    return LUA_ERRRUN;
  }
  std::vector<ActionSuggestion> ranked_actions;
  lua_pushnil(state_);
  while (Next(/*index=*/-2)) {
    const int action_id = Read<int>(/*index=*/-1) - 1;
    lua_pop(state_, 1);
    if (action_id < 0 || action_id >= response_->actions.size()) {
      TC3_LOG(ERROR) << "Invalid action index: " << action_id;
      lua_error(state_);
      return LUA_ERRRUN;
    }
    ranked_actions.push_back(response_->actions[action_id]);
  }
  lua_pop(state_, 1);
  response_->actions = ranked_actions;
  return LUA_OK;
}

bool ActionsSuggestionsLuaRanker::RankActions() {
  if (response_->actions.empty()) {
    // Nothing to do.
    return true;
  }

  if (luaL_loadbuffer(state_, ranker_code_.data(), ranker_code_.size(),
                      /*name=*/nullptr) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not load compiled ranking snippet.";
    return false;
  }

  if (lua_pcall(state_, /*nargs=*/0, /*nresults=*/1, /*errfunc=*/0) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not run ranking snippet.";
    return false;
  }

  if (RunProtected([this] { return ReadActionsRanking(); }, /*num_args=*/1) !=
      LUA_OK) {
    TC3_LOG(ERROR) << "Could not read lua result.";
    return false;
  }
  return true;
}

}  // namespace libtextclassifier3
