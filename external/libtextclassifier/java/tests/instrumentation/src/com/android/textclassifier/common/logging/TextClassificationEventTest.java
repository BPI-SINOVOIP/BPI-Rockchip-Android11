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

package com.android.textclassifier.common.logging;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.view.textclassifier.TextClassifier;
import android.widget.TextView;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import java.util.Locale;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassificationEventTest {
  @Test
  public void testTextSelectionEvent_minimal() {
    final TextClassifierEvent.TextSelectionEvent event =
        new TextClassifierEvent.TextSelectionEvent.Builder(TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_SELECTION);
    assertMinimumCommonFields(event);
    assertThat(event.getRelativeWordStartIndex()).isEqualTo(0);
    assertThat(event.getRelativeWordEndIndex()).isEqualTo(0);
    assertThat(event.getRelativeSuggestedWordStartIndex()).isEqualTo(0);
    assertThat(event.getRelativeSuggestedWordEndIndex()).isEqualTo(0);
  }

  @Test
  public void testTextSelectionEvent_full() {
    final TextClassifierEvent.TextSelectionEvent.Builder builder =
        new TextClassifierEvent.TextSelectionEvent.Builder(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    TextClassifierEvent.TextSelectionEvent event =
        builder
            .setRelativeWordStartIndex(1)
            .setRelativeWordEndIndex(2)
            .setRelativeSuggestedWordStartIndex(-1)
            .setRelativeSuggestedWordEndIndex(3)
            .build();

    assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_SELECTION);
    assertFullCommonFields(event);
    assertThat(event.getRelativeWordStartIndex()).isEqualTo(1);
    assertThat(event.getRelativeWordEndIndex()).isEqualTo(2);
    assertThat(event.getRelativeSuggestedWordStartIndex()).isEqualTo(-1);
    assertThat(event.getRelativeSuggestedWordEndIndex()).isEqualTo(3);
  }

  @Test
  public void testTextLinkifyEvent_minimal() {
    TextClassifierEvent.TextLinkifyEvent event =
        new TextClassifierEvent.TextLinkifyEvent.Builder(TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LINKIFY);
    assertMinimumCommonFields(event);
  }

  @Test
  public void testTextLinkifyEvent_full() {
    TextClassifierEvent.TextLinkifyEvent.Builder builder =
        new TextClassifierEvent.TextLinkifyEvent.Builder(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    TextClassifierEvent.TextLinkifyEvent event = builder.build();

    assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LINKIFY);
    assertFullCommonFields(event);
  }

  @Test
  public void testConversationActionsEvent_minimal() {
    TextClassifierEvent.ConversationActionsEvent event =
        new TextClassifierEvent.ConversationActionsEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    assertThat(event.getEventCategory())
        .isEqualTo(TextClassifierEvent.CATEGORY_CONVERSATION_ACTIONS);
    assertMinimumCommonFields(event);
  }

  @Test
  public void testConversationActionsEvent_full() {
    TextClassifierEvent.ConversationActionsEvent.Builder builder =
        new TextClassifierEvent.ConversationActionsEvent.Builder(
            TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    TextClassifierEvent.ConversationActionsEvent event = builder.build();

    assertThat(event.getEventCategory())
        .isEqualTo(TextClassifierEvent.CATEGORY_CONVERSATION_ACTIONS);
    assertFullCommonFields(event);
  }

  @Test
  public void testLanguageDetectionEventEvent_minimal() {
    TextClassifierEvent.LanguageDetectionEvent event =
        new TextClassifierEvent.LanguageDetectionEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION);
    assertMinimumCommonFields(event);
  }

  @Test
  public void testLanguageDetectionEvent_full() {
    TextClassifierEvent.LanguageDetectionEvent.Builder builder =
        new TextClassifierEvent.LanguageDetectionEvent.Builder(
            TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    TextClassifierEvent.LanguageDetectionEvent event = builder.build();

    assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION);
    assertFullCommonFields(event);
  }

  private static void setFullCommonFields(TextClassifierEvent.Builder<?> builder) {
    Bundle extra = new Bundle();
    extra.putString("key", "value");
    builder
        .setEventIndex(2)
        .setEntityTypes(TextClassifier.TYPE_ADDRESS)
        .setResultId("androidtc-en-v606-1234")
        .setActionIndices(1, 2, 5)
        .setExtras(extra)
        .setEventContext(
            new TextClassificationContext.Builder("pkg", TextClassifier.WIDGET_TYPE_TEXTVIEW)
                .setWidgetVersion(TextView.class.getName())
                .build())
        .setScores(0.5f)
        .setEntityTypes(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE)
        .setLocale(Locale.US);
  }

  private static void assertFullCommonFields(TextClassifierEvent event) {
    assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    assertThat(event.getEventIndex()).isEqualTo(2);
    assertThat(event.getEntityTypes())
        .asList()
        .containsExactly(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE);
    assertThat(event.getResultId()).isEqualTo("androidtc-en-v606-1234");
    assertThat(event.getActionIndices()).asList().containsExactly(1, 2, 5);
    assertThat(event.getExtras().get("key")).isEqualTo("value");
    assertThat(event.getEventContext().getPackageName()).isEqualTo("pkg");
    assertThat(event.getEventContext().getWidgetType())
        .isEqualTo(TextClassifier.WIDGET_TYPE_TEXTVIEW);
    assertThat(event.getEventContext().getWidgetVersion()).isEqualTo(TextView.class.getName());
    assertThat(event.getScores()).hasLength(1);
    assertThat(event.getScores()[0]).isEqualTo(0.5f);
    assertThat(event.getLocale().getLanguage()).isEqualTo("en");
    assertThat(event.getLocale().getCountry()).isEqualTo("US");
  }

  private static void assertMinimumCommonFields(TextClassifierEvent event) {
    assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    assertThat(event.getEventIndex()).isEqualTo(0);
    assertThat(event.getEntityTypes()).isEmpty();
    assertThat(event.getResultId()).isNull();
    assertThat(event.getActionIndices()).isEmpty();
    assertThat(event.getExtras().size()).isEqualTo(0);
    assertThat(event.getEventContext()).isNull();
    assertThat(event.getEntityTypes()).isEmpty();
    assertThat(event.getLocale()).isNull();
  }
}
