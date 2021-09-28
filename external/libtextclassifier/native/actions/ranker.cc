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

#include <functional>
#include <set>
#include <vector>

#include "actions/lua-ranker.h"
#include "actions/zlib-utils.h"
#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/lua-utils.h"

namespace libtextclassifier3 {
namespace {

void SortByScoreAndType(std::vector<ActionSuggestion>* actions) {
  std::sort(actions->begin(), actions->end(),
            [](const ActionSuggestion& a, const ActionSuggestion& b) {
              return a.score > b.score ||
                     (a.score >= b.score && a.type < b.type);
            });
}

template <typename T>
int Compare(const T& left, const T& right) {
  if (left < right) {
    return -1;
  }
  if (left > right) {
    return 1;
  }
  return 0;
}

template <>
int Compare(const std::string& left, const std::string& right) {
  return left.compare(right);
}

template <>
int Compare(const MessageTextSpan& span, const MessageTextSpan& other) {
  if (const int value = Compare(span.message_index, other.message_index)) {
    return value;
  }
  if (const int value = Compare(span.span.first, other.span.first)) {
    return value;
  }
  if (const int value = Compare(span.span.second, other.span.second)) {
    return value;
  }
  return 0;
}

bool IsSameSpan(const MessageTextSpan& span, const MessageTextSpan& other) {
  return Compare(span, other) == 0;
}

bool TextSpansIntersect(const MessageTextSpan& span,
                        const MessageTextSpan& other) {
  return span.message_index == other.message_index &&
         SpansOverlap(span.span, other.span);
}

template <>
int Compare(const ActionSuggestionAnnotation& annotation,
            const ActionSuggestionAnnotation& other) {
  if (const int value = Compare(annotation.span, other.span)) {
    return value;
  }
  if (const int value = Compare(annotation.name, other.name)) {
    return value;
  }
  if (const int value =
          Compare(annotation.entity.collection, other.entity.collection)) {
    return value;
  }
  return 0;
}

// Checks whether two annotations can be considered equivalent.
bool IsEquivalentActionAnnotation(const ActionSuggestionAnnotation& annotation,
                                  const ActionSuggestionAnnotation& other) {
  return Compare(annotation, other) == 0;
}

// Compares actions based on annotations.
int CompareAnnotationsOnly(const ActionSuggestion& action,
                           const ActionSuggestion& other) {
  if (const int value =
          Compare(action.annotations.size(), other.annotations.size())) {
    return value;
  }
  for (int i = 0; i < action.annotations.size(); i++) {
    if (const int value =
            Compare(action.annotations[i], other.annotations[i])) {
      return value;
    }
  }
  return 0;
}

// Checks whether two actions have the same annotations.
bool HaveEquivalentAnnotations(const ActionSuggestion& action,
                               const ActionSuggestion& other) {
  return CompareAnnotationsOnly(action, other) == 0;
}

template <>
int Compare(const ActionSuggestion& action, const ActionSuggestion& other) {
  if (const int value = Compare(action.type, other.type)) {
    return value;
  }
  if (const int value = Compare(action.response_text, other.response_text)) {
    return value;
  }
  if (const int value = Compare(action.serialized_entity_data,
                                other.serialized_entity_data)) {
    return value;
  }
  return CompareAnnotationsOnly(action, other);
}

// Checks whether two action suggestions can be considered equivalent.
bool IsEquivalentActionSuggestion(const ActionSuggestion& action,
                                  const ActionSuggestion& other) {
  return Compare(action, other) == 0;
}

// Checks whether any action is equivalent to the given one.
bool IsAnyActionEquivalent(const ActionSuggestion& action,
                           const std::vector<ActionSuggestion>& actions) {
  for (const ActionSuggestion& other : actions) {
    if (IsEquivalentActionSuggestion(action, other)) {
      return true;
    }
  }
  return false;
}

bool IsConflicting(const ActionSuggestionAnnotation& annotation,
                   const ActionSuggestionAnnotation& other) {
  // Two annotations are conflicting if they are different but refer to
  // overlapping spans in the conversation.
  return (!IsEquivalentActionAnnotation(annotation, other) &&
          TextSpansIntersect(annotation.span, other.span));
}

// Checks whether two action suggestions can be considered conflicting.
bool IsConflictingActionSuggestion(const ActionSuggestion& action,
                                   const ActionSuggestion& other) {
  // Actions are considered conflicting, iff they refer to the same text span,
  // but were not generated from the same annotation.
  if (action.annotations.empty() || other.annotations.empty()) {
    return false;
  }
  for (const ActionSuggestionAnnotation& annotation : action.annotations) {
    for (const ActionSuggestionAnnotation& other_annotation :
         other.annotations) {
      if (IsConflicting(annotation, other_annotation)) {
        return true;
      }
    }
  }
  return false;
}

// Checks whether any action is considered conflicting with the given one.
bool IsAnyActionConflicting(const ActionSuggestion& action,
                            const std::vector<ActionSuggestion>& actions) {
  for (const ActionSuggestion& other : actions) {
    if (IsConflictingActionSuggestion(action, other)) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::unique_ptr<ActionsSuggestionsRanker>
ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
    const RankingOptions* options, ZlibDecompressor* decompressor,
    const std::string& smart_reply_action_type) {
  auto ranker = std::unique_ptr<ActionsSuggestionsRanker>(
      new ActionsSuggestionsRanker(options, smart_reply_action_type));

  if (!ranker->InitializeAndValidate(decompressor)) {
    TC3_LOG(ERROR) << "Could not initialize action ranker.";
    return nullptr;
  }

  return ranker;
}

bool ActionsSuggestionsRanker::InitializeAndValidate(
    ZlibDecompressor* decompressor) {
  if (options_ == nullptr) {
    TC3_LOG(ERROR) << "No ranking options specified.";
    return false;
  }

  std::string lua_ranking_script;
  if (GetUncompressedString(options_->lua_ranking_script(),
                            options_->compressed_lua_ranking_script(),
                            decompressor, &lua_ranking_script) &&
      !lua_ranking_script.empty()) {
    if (!Compile(lua_ranking_script, &lua_bytecode_)) {
      TC3_LOG(ERROR) << "Could not precompile lua ranking snippet.";
      return false;
    }
  }

  return true;
}

bool ActionsSuggestionsRanker::RankActions(
    const Conversation& conversation, ActionsSuggestionsResponse* response,
    const reflection::Schema* entity_data_schema,
    const reflection::Schema* annotations_entity_data_schema) const {
  if (options_->deduplicate_suggestions() ||
      options_->deduplicate_suggestions_by_span()) {
    // First order suggestions by priority score for deduplication.
    std::sort(
        response->actions.begin(), response->actions.end(),
        [](const ActionSuggestion& a, const ActionSuggestion& b) {
          return a.priority_score > b.priority_score ||
                 (a.priority_score >= b.priority_score && a.score > b.score);
        });

    // Deduplicate, keeping the higher score actions.
    if (options_->deduplicate_suggestions()) {
      std::vector<ActionSuggestion> deduplicated_actions;
      for (const ActionSuggestion& candidate : response->actions) {
        // Check whether we already have an equivalent action.
        if (!IsAnyActionEquivalent(candidate, deduplicated_actions)) {
          deduplicated_actions.push_back(std::move(candidate));
        }
      }
      response->actions = std::move(deduplicated_actions);
    }

    // Resolve conflicts between conflicting actions referring to the same
    // text span.
    if (options_->deduplicate_suggestions_by_span()) {
      std::vector<ActionSuggestion> deduplicated_actions;
      for (const ActionSuggestion& candidate : response->actions) {
        // Check whether we already have a conflicting action.
        if (!IsAnyActionConflicting(candidate, deduplicated_actions)) {
          deduplicated_actions.push_back(std::move(candidate));
        }
      }
      response->actions = std::move(deduplicated_actions);
    }
  }

  // Suppress smart replies if actions are present.
  if (options_->suppress_smart_replies_with_actions()) {
    std::vector<ActionSuggestion> non_smart_reply_actions;
    for (const ActionSuggestion& action : response->actions) {
      if (action.type != smart_reply_action_type_) {
        non_smart_reply_actions.push_back(std::move(action));
      }
    }
    response->actions = std::move(non_smart_reply_actions);
  }

  // Group by annotation if specified.
  if (options_->group_by_annotations()) {
    auto group_id = std::map<
        ActionSuggestion, int,
        std::function<bool(const ActionSuggestion&, const ActionSuggestion&)>>{
        [](const ActionSuggestion& action, const ActionSuggestion& other) {
          return (CompareAnnotationsOnly(action, other) < 0);
        }};
    typedef std::vector<ActionSuggestion> ActionSuggestionGroup;
    std::vector<ActionSuggestionGroup> groups;

    // Group actions by the annotation set they are based of.
    for (const ActionSuggestion& action : response->actions) {
      // Treat actions with no annotations idependently.
      if (action.annotations.empty()) {
        groups.emplace_back(1, action);
        continue;
      }

      auto it = group_id.find(action);
      if (it != group_id.end()) {
        groups[it->second].push_back(action);
      } else {
        group_id[action] = groups.size();
        groups.emplace_back(1, action);
      }
    }

    // Sort within each group by score.
    for (std::vector<ActionSuggestion>& group : groups) {
      SortByScoreAndType(&group);
    }

    // Sort groups by maximum score.
    std::sort(groups.begin(), groups.end(),
              [](const std::vector<ActionSuggestion>& a,
                 const std::vector<ActionSuggestion>& b) {
                return a.begin()->score > b.begin()->score ||
                       (a.begin()->score >= b.begin()->score &&
                        a.begin()->type < b.begin()->type);
              });

    // Flatten result.
    const size_t num_actions = response->actions.size();
    response->actions.clear();
    response->actions.reserve(num_actions);
    for (const std::vector<ActionSuggestion>& actions : groups) {
      response->actions.insert(response->actions.end(), actions.begin(),
                               actions.end());
    }

  } else {
    // Order suggestions independently by score.
    SortByScoreAndType(&response->actions);
  }

  // Run lua ranking snippet, if provided.
  if (!lua_bytecode_.empty()) {
    auto lua_ranker = ActionsSuggestionsLuaRanker::Create(
        conversation, lua_bytecode_, entity_data_schema,
        annotations_entity_data_schema, response);
    if (lua_ranker == nullptr || !lua_ranker->RankActions()) {
      TC3_LOG(ERROR) << "Could not run lua ranking snippet.";
      return false;
    }
  }

  return true;
}

}  // namespace libtextclassifier3
