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

#include "actions/utils.h"

#include "utils/base/logging.h"
#include "utils/normalization.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

void FillSuggestionFromSpec(const ActionSuggestionSpec* action,
                            ReflectiveFlatbuffer* entity_data,
                            ActionSuggestion* suggestion) {
  if (action != nullptr) {
    suggestion->score = action->score();
    suggestion->priority_score = action->priority_score();
    if (action->type() != nullptr) {
      suggestion->type = action->type()->str();
    }
    if (action->response_text() != nullptr) {
      suggestion->response_text = action->response_text()->str();
    }
    if (action->serialized_entity_data() != nullptr) {
      TC3_CHECK_NE(entity_data, nullptr);
      entity_data->MergeFromSerializedFlatbuffer(
          StringPiece(action->serialized_entity_data()->data(),
                      action->serialized_entity_data()->size()));
    }
    if (action->entity_data() != nullptr) {
      TC3_CHECK_NE(entity_data, nullptr);
      entity_data->MergeFrom(
          reinterpret_cast<const flatbuffers::Table*>(action->entity_data()));
    }
  }
  if (entity_data != nullptr && entity_data->HasExplicitlySetFields()) {
    suggestion->serialized_entity_data = entity_data->Serialize();
  }
}

void SuggestTextRepliesFromCapturingMatch(
    const ReflectiveFlatbufferBuilder* entity_data_builder,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    const UnicodeText& match_text, const std::string& smart_reply_action_type,
    std::vector<ActionSuggestion>* actions) {
  if (group->text_reply() != nullptr) {
    ActionSuggestion suggestion;
    suggestion.response_text = match_text.ToUTF8String();
    suggestion.type = smart_reply_action_type;
    std::unique_ptr<ReflectiveFlatbuffer> entity_data =
        entity_data_builder != nullptr ? entity_data_builder->NewRoot()
                                       : nullptr;
    FillSuggestionFromSpec(group->text_reply(), entity_data.get(), &suggestion);
    actions->push_back(suggestion);
  }
}

UnicodeText NormalizeMatchText(
    const UniLib& unilib,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    StringPiece match_text) {
  UnicodeText normalized_match_text =
      UTF8ToUnicodeText(match_text, /*do_copy=*/false);
  if (group->normalization_options() != nullptr) {
    normalized_match_text = NormalizeText(
        unilib, group->normalization_options(), normalized_match_text);
  }
  return normalized_match_text;
}

bool FillAnnotationFromCapturingMatch(
    const CodepointSpan& span,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    const int message_index, StringPiece match_text,
    ActionSuggestionAnnotation* annotation) {
  if (group->annotation_name() == nullptr &&
      group->annotation_type() == nullptr) {
    return false;
  }
  annotation->span.span = span;
  annotation->span.message_index = message_index;
  annotation->span.text = match_text.ToString();
  if (group->annotation_name() != nullptr) {
    annotation->name = group->annotation_name()->str();
  }
  if (group->annotation_type() != nullptr) {
    annotation->entity.collection = group->annotation_type()->str();
  }
  return true;
}

bool MergeEntityDataFromCapturingMatch(
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    StringPiece match_text, ReflectiveFlatbuffer* buffer) {
  if (group->entity_field() != nullptr) {
    if (!buffer->ParseAndSet(group->entity_field(), match_text.ToString())) {
      TC3_LOG(ERROR) << "Could not set entity data from rule capturing group.";
      return false;
    }
  }
  if (group->entity_data() != nullptr) {
    if (!buffer->MergeFrom(reinterpret_cast<const flatbuffers::Table*>(
            group->entity_data()))) {
      TC3_LOG(ERROR) << "Could not set entity data for capturing match.";
      return false;
    }
  }
  return true;
}

}  // namespace libtextclassifier3
