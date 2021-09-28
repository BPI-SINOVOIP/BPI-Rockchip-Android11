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

package com.android.textclassifier.notification;

import android.app.Notification;
import android.service.notification.NotificationAssistantService;
import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import com.google.common.collect.ImmutableMap;
import java.util.List;
import java.util.Map;

/** Logs events related to a {@link TextClassifier} result. */
final class SmartSuggestionsLogSession {

  private final TextClassificationContext textClassificationContext;
  private final String resultId;
  private final ImmutableMap<CharSequence, Float> repliesScores;
  private final TextClassifier textClassifier;
  private boolean isSeenEventLogged;

  /**
   * Creates a session for logging purpose.
   *
   * @param resultId the result id from a {@link TextClassifier} result object.
   * @param repliesScores a map contains suggested replies and their scores.
   * @param textClassifier a text classifier that the log will be sent to. This instance will take
   *     the ownership of this text classifier, do not further interact with it directly.
   * @param textClassificationContext a context where the {@link TextClassifier} API is performed.
   */
  SmartSuggestionsLogSession(
      String resultId,
      Map<CharSequence, Float> repliesScores,
      TextClassifier textClassifier,
      TextClassificationContext textClassificationContext) {
    this.resultId = resultId;
    this.repliesScores = ImmutableMap.copyOf(repliesScores);
    this.textClassifier = textClassifier;
    this.textClassificationContext = textClassificationContext;
  }

  /**
   * Notifies that a notification has been expanded or collapsed.
   *
   * @param isExpanded true for when a notification is expanded, false for when it is collapsed.
   */
  void onNotificationExpansionChanged(boolean isExpanded) {
    if (!isExpanded) {
      return;
    }
    // Only report if this is the first time the user sees these suggestions.
    if (isSeenEventLogged) {
      return;
    }
    isSeenEventLogged = true;
    TextClassifierEvent textClassifierEvent =
        createTextClassifierEventBuilder(TextClassifierEvent.TYPE_ACTIONS_SHOWN, resultId).build();
    // TODO(tonymak): If possible, report which replies / actions are actually seen by user.
    textClassifier.onTextClassifierEvent(textClassifierEvent);
  }

  /**
   * Notifies that a suggested text reply has been sent.
   *
   * @param action the action that was just clicked
   * @param source the source that provided the reply, e.g. SOURCE_FROM_ASSISTANT
   */
  void onActionClicked(Notification.Action action, int source) {
    if (source != NotificationAssistantService.SOURCE_FROM_ASSISTANT) {
      return;
    }
    String actionType = action.getExtras().getString(SmartSuggestionsHelper.KEY_ACTION_TYPE);
    if (actionType == null) {
      return;
    }
    TextClassifierEvent textClassifierEvent =
        createTextClassifierEventBuilder(TextClassifierEvent.TYPE_SMART_ACTION, resultId)
            .setEntityTypes(actionType)
            .build();
    textClassifier.onTextClassifierEvent(textClassifierEvent);
  }

  /**
   * Notifies that a suggested reply has been sent.
   *
   * @param reply the reply that is just sent
   * @param source the source that provided the reply, e.g. SOURCE_FROM_ASSISTANT
   */
  void onSuggestedReplySent(CharSequence reply, int source) {
    if (source != NotificationAssistantService.SOURCE_FROM_ASSISTANT) {
      return;
    }
    TextClassifierEvent textClassifierEvent =
        createTextClassifierEventBuilder(TextClassifierEvent.TYPE_SMART_ACTION, resultId)
            .setEntityTypes(ConversationAction.TYPE_TEXT_REPLY)
            .setScores(repliesScores.getOrDefault(reply, 0f))
            .build();
    textClassifier.onTextClassifierEvent(textClassifierEvent);
  }

  /** Notifies that a direct reply has been sent from a notification. */
  void onDirectReplied() {
    TextClassifierEvent textClassifierEvent =
        createTextClassifierEventBuilder(TextClassifierEvent.TYPE_MANUAL_REPLY, resultId).build();
    textClassifier.onTextClassifierEvent(textClassifierEvent);
  }

  /** Notifies that some suggestions have been generated. */
  void onSuggestionsGenerated(List<ConversationAction> generatedActions) {
    TextClassifierEvent textClassifierEvent =
        createTextClassifierEventBuilder(TextClassifierEvent.TYPE_ACTIONS_GENERATED, resultId)
            .setEntityTypes(
                generatedActions.stream().map(ConversationAction::getType).toArray(String[]::new))
            .build();
    textClassifier.onTextClassifierEvent(textClassifierEvent);
  }

  /** Destroys this session. Do not call any method in this class after this is called. */
  void destroy() {
    textClassifier.destroy();
  }

  private TextClassifierEvent.ConversationActionsEvent.Builder createTextClassifierEventBuilder(
      int eventType, String resultId) {
    return new TextClassifierEvent.ConversationActionsEvent.Builder(eventType)
        .setEventContext(this.textClassificationContext)
        .setResultId(resultId);
  }
}
