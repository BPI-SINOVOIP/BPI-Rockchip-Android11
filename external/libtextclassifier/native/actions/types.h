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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_TYPES_H_
#define LIBTEXTCLASSIFIER_ACTIONS_TYPES_H_

#include <map>
#include <string>
#include <vector>

#include "actions/actions-entity-data_generated.h"
#include "annotator/types.h"
#include "utils/flatbuffers.h"

namespace libtextclassifier3 {

// A text span in the conversation.
struct MessageTextSpan {
  explicit MessageTextSpan() = default;
  MessageTextSpan(const int message_index, const CodepointSpan span,
                  const std::string& text)
      : message_index(message_index), span(span), text(text) {}

  // The referenced message.
  // -1 if not referencing a particular message in the provided input.
  int message_index = kInvalidIndex;

  // The span within the reference message.
  // (-1, -1) if not referencing a particular location.
  CodepointSpan span = CodepointSpan{kInvalidIndex, kInvalidIndex};

  // The span text.
  std::string text;
};

// An entity associated with an action.
struct ActionSuggestionAnnotation {
  MessageTextSpan span;
  ClassificationResult entity;

  // Optional annotation name.
  std::string name;
};

// Action suggestion that contains a response text and the type of the response.
struct ActionSuggestion {
  // Text of the action suggestion.
  std::string response_text;

  // Type of the action suggestion.
  std::string type;

  // Score.
  float score = 0.f;

  // Priority score for internal conflict resolution.
  float priority_score = 0.f;

  // The associated annotations.
  std::vector<ActionSuggestionAnnotation> annotations;

  // Extras information.
  std::string serialized_entity_data;

  const ActionsEntityData* entity_data() {
    return LoadAndVerifyFlatbuffer<ActionsEntityData>(
        serialized_entity_data.data(), serialized_entity_data.size());
  }
};

// Actions suggestions result containing meta - information and the suggested
// actions.
struct ActionsSuggestionsResponse {
  // The sensitivity assessment.
  float sensitivity_score = -1.f;
  float triggering_score = -1.f;

  // Whether the output was suppressed by the sensitivity threshold.
  bool output_filtered_sensitivity = false;

  // Whether the output was suppressed by the triggering score threshold.
  bool output_filtered_min_triggering_score = false;

  // Whether the output was suppressed by the low confidence patterns.
  bool output_filtered_low_confidence = false;

  // Whether the output was suppressed due to locale mismatch.
  bool output_filtered_locale_mismatch = false;

  // The suggested actions.
  std::vector<ActionSuggestion> actions;
};

// Represents a single message in the conversation.
struct ConversationMessage {
  // User ID distinguishing the user from other users in the conversation.
  int user_id = 0;

  // Text of the message.
  std::string text;

  // Reference time of this message.
  int64 reference_time_ms_utc = 0;

  // Timezone in which the input text was written (format as accepted by ICU).
  std::string reference_timezone;

  // Annotations on the text.
  std::vector<AnnotatedSpan> annotations;

  // Comma-separated list of BCP 47 language tags of the message.
  std::string detected_text_language_tags;
};

// Conversation between multiple users.
struct Conversation {
  // Sequence of messages that were exchanged in the conversation.
  std::vector<ConversationMessage> messages;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_TYPES_H_
