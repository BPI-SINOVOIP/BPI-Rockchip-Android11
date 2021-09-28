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

package com.android.textclassifier.common.statsd;

import static com.google.common.truth.Truth.assertThat;

import android.icu.util.ULocale;
import android.os.Bundle;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassifier;
import android.widget.TextView;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.common.logging.TextClassifierEvent;
import com.android.textclassifier.common.logging.TextClassifierEvent.TextSelectionEvent;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassifierEventConverterTest {
  private static final float TOLERANCE = 0.000001f;

  @Test
  public void testTextSelectionEvent_minimal() {
    final android.view.textclassifier.TextClassifierEvent.TextSelectionEvent event =
        new android.view.textclassifier.TextClassifierEvent.TextSelectionEvent.Builder(
                android.view.textclassifier.TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    TextSelectionEvent result =
        (TextSelectionEvent) TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_SELECTION);
    assertMinimumCommonFields(result);
    assertThat(result.getRelativeWordStartIndex()).isEqualTo(0);
    assertThat(result.getRelativeWordEndIndex()).isEqualTo(0);
    assertThat(result.getRelativeSuggestedWordStartIndex()).isEqualTo(0);
    assertThat(result.getRelativeSuggestedWordEndIndex()).isEqualTo(0);
  }

  @Test
  public void testTextSelectionEvent_full() {
    final android.view.textclassifier.TextClassifierEvent.TextSelectionEvent.Builder builder =
        new android.view.textclassifier.TextClassifierEvent.TextSelectionEvent.Builder(
            android.view.textclassifier.TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    android.view.textclassifier.TextClassifierEvent.TextSelectionEvent event =
        builder
            .setRelativeWordStartIndex(1)
            .setRelativeWordEndIndex(2)
            .setRelativeSuggestedWordStartIndex(-1)
            .setRelativeSuggestedWordEndIndex(3)
            .build();

    TextClassifierEvent.TextSelectionEvent result =
        (TextClassifierEvent.TextSelectionEvent) TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_SELECTION);
    assertFullCommonFields(result);
    assertThat(result.getRelativeWordStartIndex()).isEqualTo(1);
    assertThat(result.getRelativeWordEndIndex()).isEqualTo(2);
    assertThat(result.getRelativeSuggestedWordStartIndex()).isEqualTo(-1);
    assertThat(result.getRelativeSuggestedWordEndIndex()).isEqualTo(3);
  }

  @Test
  public void testTextLinkifyEvent_minimal() {
    android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent event =
        new android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    TextClassifierEvent.TextLinkifyEvent result =
        (TextClassifierEvent.TextLinkifyEvent) TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LINKIFY);
    assertMinimumCommonFields(result);
  }

  @Test
  public void testTextLinkifyEvent_full() {
    android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent.Builder builder =
        new android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent.Builder(
            android.view.textclassifier.TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent event = builder.build();

    TextClassifierEvent.TextLinkifyEvent result =
        (TextClassifierEvent.TextLinkifyEvent) TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LINKIFY);
    assertFullCommonFields(result);
  }

  @Test
  public void testConversationActionsEvent_minimal() {
    android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent event =
        new android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    TextClassifierEvent.ConversationActionsEvent result =
        (TextClassifierEvent.ConversationActionsEvent)
            TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory())
        .isEqualTo(TextClassifierEvent.CATEGORY_CONVERSATION_ACTIONS);
    assertMinimumCommonFields(result);
  }

  @Test
  public void testConversationActionsEvent_full() {
    android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent.Builder builder =
        new android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent.Builder(
            android.view.textclassifier.TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent event =
        builder.build();

    TextClassifierEvent.ConversationActionsEvent result =
        (TextClassifierEvent.ConversationActionsEvent)
            TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory())
        .isEqualTo(TextClassifierEvent.CATEGORY_CONVERSATION_ACTIONS);
    assertFullCommonFields(result);
  }

  @Test
  public void testLanguageDetectionEventEvent_minimal() {
    android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent event =
        new android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
            .build();

    TextClassifierEvent.LanguageDetectionEvent result =
        (TextClassifierEvent.LanguageDetectionEvent)
            TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory())
        .isEqualTo(TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION);
    assertMinimumCommonFields(result);
  }

  @Test
  public void testLanguageDetectionEvent_full() {
    android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent.Builder builder =
        new android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent.Builder(
            android.view.textclassifier.TextClassifierEvent.TYPE_ACTIONS_SHOWN);
    setFullCommonFields(builder);
    android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent event = builder.build();

    TextClassifierEvent.LanguageDetectionEvent result =
        (TextClassifierEvent.LanguageDetectionEvent)
            TextClassifierEventConverter.fromPlatform(event);

    assertThat(result.getEventCategory())
        .isEqualTo(TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION);
    assertFullCommonFields(result);
  }

  private static void setFullCommonFields(
      android.view.textclassifier.TextClassifierEvent.Builder<?> builder) {
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
        .setLocale(ULocale.US);
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
    assertThat(event.getScores()[0]).isWithin(TOLERANCE).of(0.5f);
    assertThat(event.getLocale().toLanguageTag()).isEqualTo("en-US");
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
