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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_LUA_ACTIONS_H_
#define LIBTEXTCLASSIFIER_ACTIONS_LUA_ACTIONS_H_

#include "actions/actions_model_generated.h"
#include "actions/types.h"
#include "utils/lua-utils.h"
#include "utils/tensor-view.h"
#include "utils/tflite-model-executor.h"

namespace libtextclassifier3 {

// Lua backed actions suggestions.
class LuaActionsSuggestions : public LuaEnvironment {
 public:
  static std::unique_ptr<LuaActionsSuggestions> CreateLuaActionsSuggestions(
      const std::string& snippet, const Conversation& conversation,
      const TfLiteModelExecutor* model_executor,
      const TensorflowLiteModelSpec* model_spec,
      const tflite::Interpreter* interpreter,
      const reflection::Schema* actions_entity_data_schema,
      const reflection::Schema* annotations_entity_data_schema);

  bool SuggestActions(std::vector<ActionSuggestion>* actions);

 private:
  LuaActionsSuggestions(
      const std::string& snippet, const Conversation& conversation,
      const TfLiteModelExecutor* model_executor,
      const TensorflowLiteModelSpec* model_spec,
      const tflite::Interpreter* interpreter,
      const reflection::Schema* actions_entity_data_schema,
      const reflection::Schema* annotations_entity_data_schema);

  bool Initialize();

  template <typename T>
  void PushTensor(const TensorView<T>* tensor) const {
    PushIterator(tensor ? tensor->size() : 0,
                 [this, tensor](const int64 index) {
                   Push(tensor->data()[index]);
                   return 1;  // Num. values pushed.
                 });
  }

  const std::string& snippet_;
  const Conversation& conversation_;
  TensorView<float> actions_scores_;
  TensorView<float> smart_reply_scores_;
  TensorView<float> sensitivity_score_;
  TensorView<float> triggering_score_;
  const std::vector<std::string> smart_replies_;
  const reflection::Schema* actions_entity_data_schema_;
  const reflection::Schema* annotations_entity_data_schema_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_LUA_ACTIONS_H_
