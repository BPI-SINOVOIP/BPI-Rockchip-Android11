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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_RANKER_H_
#define LIBTEXTCLASSIFIER_ACTIONS_RANKER_H_

#include <memory>

#include "actions/actions_model_generated.h"
#include "actions/types.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3 {

// Ranking and filtering of actions suggestions.
class ActionsSuggestionsRanker {
 public:
  static std::unique_ptr<ActionsSuggestionsRanker>
  CreateActionsSuggestionsRanker(const RankingOptions* options,
                                 ZlibDecompressor* decompressor,
                                 const std::string& smart_reply_action_type);

  // Rank and filter actions.
  bool RankActions(
      const Conversation& conversation, ActionsSuggestionsResponse* response,
      const reflection::Schema* entity_data_schema = nullptr,
      const reflection::Schema* annotations_entity_data_schema = nullptr) const;

 private:
  explicit ActionsSuggestionsRanker(const RankingOptions* options,
                                    const std::string& smart_reply_action_type)
      : options_(options), smart_reply_action_type_(smart_reply_action_type) {}

  bool InitializeAndValidate(ZlibDecompressor* decompressor);

  const RankingOptions* const options_;
  std::string lua_bytecode_;
  std::string smart_reply_action_type_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_RANKER_H_
