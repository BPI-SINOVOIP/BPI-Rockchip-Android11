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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_LUA_RANKER_H_
#define LIBTEXTCLASSIFIER_ACTIONS_LUA_RANKER_H_

#include <memory>
#include <string>

#include "actions/types.h"
#include "utils/lua-utils.h"

namespace libtextclassifier3 {

// Lua backed action suggestion ranking.
class ActionsSuggestionsLuaRanker : public LuaEnvironment {
 public:
  static std::unique_ptr<ActionsSuggestionsLuaRanker> Create(
      const Conversation& conversation, const std::string& ranker_code,
      const reflection::Schema* entity_data_schema,
      const reflection::Schema* annotations_entity_data_schema,
      ActionsSuggestionsResponse* response);

  bool RankActions();

 private:
  explicit ActionsSuggestionsLuaRanker(
      const Conversation& conversation, const std::string& ranker_code,
      const reflection::Schema* actions_entity_data_schema,
      const reflection::Schema* annotations_entity_data_schema,
      ActionsSuggestionsResponse* response)
      : conversation_(conversation),
        ranker_code_(ranker_code),
        actions_entity_data_schema_(actions_entity_data_schema),
        annotations_entity_data_schema_(annotations_entity_data_schema),
        response_(response) {}

  bool Initialize();

  // Reads ranking results from the lua stack.
  int ReadActionsRanking();

  const Conversation& conversation_;
  const std::string& ranker_code_;
  const reflection::Schema* actions_entity_data_schema_;
  const reflection::Schema* annotations_entity_data_schema_;
  ActionsSuggestionsResponse* response_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_LUA_RANKER_H_
