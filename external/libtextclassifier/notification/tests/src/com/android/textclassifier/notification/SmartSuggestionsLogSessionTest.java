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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.service.notification.NotificationAssistantService;
import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class SmartSuggestionsLogSessionTest {
  private static final String RESULT_ID = "resultId";
  private static final String REPLY = "reply";
  private static final float SCORE = 0.5f;

  @Mock private TextClassifier textClassifier;
  private SmartSuggestionsLogSession session;

  @Before
  public void setup() {
    MockitoAnnotations.initMocks(this);

    session =
        new SmartSuggestionsLogSession(
            RESULT_ID,
            ImmutableMap.of(REPLY, SCORE),
            textClassifier,
            new TextClassificationContext.Builder(
                    "pkg.name", TextClassifier.WIDGET_TYPE_NOTIFICATION)
                .build());
  }

  @Test
  public void onActionClicked() {
    session.onActionClicked(
        createNotificationAction(), NotificationAssistantService.SOURCE_FROM_ASSISTANT);

    TextClassifierEvent textClassifierEvent = getLoggedEvent();
    assertThat(textClassifierEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_SMART_ACTION);
    assertThat(textClassifierEvent.getResultId()).isEqualTo(RESULT_ID);
    assertThat(textClassifierEvent.getEntityTypes())
        .asList()
        .containsExactly(ConversationAction.TYPE_CALL_PHONE);
  }

  @Test
  public void onActionClicked_sourceFromApp() {
    session.onActionClicked(
        createNotificationAction(), NotificationAssistantService.SOURCE_FROM_APP);

    verify(textClassifier, never()).onTextClassifierEvent(any());
  }

  private static Notification.Action createNotificationAction() {
    Bundle actionExtras = new Bundle();
    actionExtras.putString(
        SmartSuggestionsHelper.KEY_ACTION_TYPE, ConversationAction.TYPE_CALL_PHONE);
    return new Notification.Action.Builder(
            Icon.createWithData(new byte[0], 0, 0),
            "Label",
            PendingIntent.getActivity(
                ApplicationProvider.getApplicationContext(), 0, new Intent(), 0))
        .addExtras(actionExtras)
        .build();
  }

  @Test
  public void onDirectReplied() {
    session.onDirectReplied();

    TextClassifierEvent textClassifierEvent = getLoggedEvent();
    assertThat(textClassifierEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_MANUAL_REPLY);
    assertThat(textClassifierEvent.getResultId()).isEqualTo(RESULT_ID);
  }

  @Test
  public void onNotificationExpansionChanged() {
    session.onNotificationExpansionChanged(/* isExpanded= */ true);

    TextClassifierEvent textClassifierEvent = getLoggedEvent();
    assertThat(textClassifierEvent.getEventType())
        .isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    assertThat(textClassifierEvent.getResultId()).isEqualTo(RESULT_ID);
  }

  @Test
  public void onNotificationExpansionChanged_loggedOnce() {
    session.onNotificationExpansionChanged(/* isExpanded= */ true);
    session.onNotificationExpansionChanged(/* isExpanded= */ true);

    ArgumentCaptor<TextClassifierEvent> argumentCaptor =
        ArgumentCaptor.forClass(TextClassifierEvent.class);
    verify(textClassifier).onTextClassifierEvent(argumentCaptor.capture());
    assertThat(argumentCaptor.getAllValues()).hasSize(1);
  }

  @Test
  public void onNotificationExpansionChanged_collapsed() {
    session.onNotificationExpansionChanged(/* isExpanded= */ false);

    verify(textClassifier, never()).onTextClassifierEvent(any());
  }

  @Test
  public void onSuggestedReplySent() {
    session.onSuggestedReplySent(REPLY, NotificationAssistantService.SOURCE_FROM_ASSISTANT);

    TextClassifierEvent textClassifierEvent = getLoggedEvent();
    assertThat(textClassifierEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_SMART_ACTION);
    assertThat(textClassifierEvent.getResultId()).isEqualTo(RESULT_ID);
    assertThat(textClassifierEvent.getScores()).usingExactEquality().containsExactly(SCORE);
  }

  @Test
  public void onSuggestedReplySent_sourceFromApp() {
    session.onSuggestedReplySent(REPLY, NotificationAssistantService.SOURCE_FROM_APP);

    verify(textClassifier, never()).onTextClassifierEvent(any());
  }

  @Test
  public void onSuggestionsGenerated() {
    ConversationAction callPhoneAction =
        new ConversationAction.Builder(ConversationAction.TYPE_CALL_PHONE).build();
    ConversationAction openUrlAction =
        new ConversationAction.Builder(ConversationAction.TYPE_OPEN_URL).build();

    session.onSuggestionsGenerated(ImmutableList.of(callPhoneAction, openUrlAction));

    TextClassifierEvent textClassifierEvent = getLoggedEvent();
    assertThat(textClassifierEvent.getEventType())
        .isEqualTo(TextClassifierEvent.TYPE_ACTIONS_GENERATED);
    assertThat(textClassifierEvent.getEntityTypes())
        .asList()
        .containsExactly(ConversationAction.TYPE_CALL_PHONE, ConversationAction.TYPE_OPEN_URL);
  }

  private TextClassifierEvent getLoggedEvent() {
    ArgumentCaptor<TextClassifierEvent> argumentCaptor =
        ArgumentCaptor.forClass(TextClassifierEvent.class);
    verify(textClassifier).onTextClassifierEvent(argumentCaptor.capture());
    return argumentCaptor.getValue();
  }
}
