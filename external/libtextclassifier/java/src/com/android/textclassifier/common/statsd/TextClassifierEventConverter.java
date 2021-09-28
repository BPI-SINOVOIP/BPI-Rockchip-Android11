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

import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.logging.TextClassificationContext;
import com.android.textclassifier.common.logging.TextClassifierEvent;
import com.android.textclassifier.common.logging.TextClassifierEvent.ConversationActionsEvent;
import com.android.textclassifier.common.logging.TextClassifierEvent.LanguageDetectionEvent;
import com.android.textclassifier.common.logging.TextClassifierEvent.TextLinkifyEvent;
import com.android.textclassifier.common.logging.TextClassifierEvent.TextSelectionEvent;
import javax.annotation.Nullable;

/**
 * Converts between {@link TextClassifierEvent} and {@link
 * android.view.textclassifier.TextClassifierEvent}.
 */
public final class TextClassifierEventConverter {
  private static final String TAG = "TextClassifierEventConv";

  /**
   * Converts a {@link android.view.textclassifier.TextClassifierEvent} object to a {@link
   * TextClassifierEvent}. Returns {@code null} if conversion fails.
   */
  @Nullable
  public static TextClassifierEvent fromPlatform(
      @Nullable android.view.textclassifier.TextClassifierEvent textClassifierEvent) {
    if (textClassifierEvent == null) {
      return null;
    }
    if (textClassifierEvent
        instanceof android.view.textclassifier.TextClassifierEvent.TextSelectionEvent) {
      return fromPlatform(
          (android.view.textclassifier.TextClassifierEvent.TextSelectionEvent) textClassifierEvent);
    } else if (textClassifierEvent
        instanceof android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent) {
      return fromPlatform(
          (android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent) textClassifierEvent);
    } else if (textClassifierEvent
        instanceof android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent) {
      return fromPlatform(
          (android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent)
              textClassifierEvent);
    } else if (textClassifierEvent
        instanceof android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent) {
      return fromPlatform(
          (android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent)
              textClassifierEvent);
    }
    TcLog.w(TAG, "Unexpected event: " + textClassifierEvent);
    return null;
  }

  private static TextSelectionEvent fromPlatform(
      android.view.textclassifier.TextClassifierEvent.TextSelectionEvent textSelectionEvent) {
    TextSelectionEvent.Builder builder =
        new TextSelectionEvent.Builder(textSelectionEvent.getEventType());
    copyCommonFields(textSelectionEvent, builder);
    return builder
        .setRelativeWordStartIndex(textSelectionEvent.getRelativeWordStartIndex())
        .setRelativeWordEndIndex(textSelectionEvent.getRelativeWordEndIndex())
        .setRelativeSuggestedWordStartIndex(textSelectionEvent.getRelativeSuggestedWordStartIndex())
        .setRelativeSuggestedWordEndIndex(textSelectionEvent.getRelativeSuggestedWordEndIndex())
        .build();
  }

  private static TextLinkifyEvent fromPlatform(
      android.view.textclassifier.TextClassifierEvent.TextLinkifyEvent textLinkifyEvent) {
    TextLinkifyEvent.Builder builder =
        new TextLinkifyEvent.Builder(textLinkifyEvent.getEventType());
    copyCommonFields(textLinkifyEvent, builder);
    return builder.build();
  }

  private static ConversationActionsEvent fromPlatform(
      android.view.textclassifier.TextClassifierEvent.ConversationActionsEvent
          conversationActionsEvent) {
    ConversationActionsEvent.Builder builder =
        new ConversationActionsEvent.Builder(conversationActionsEvent.getEventType());
    copyCommonFields(conversationActionsEvent, builder);
    return builder.build();
  }

  private static LanguageDetectionEvent fromPlatform(
      android.view.textclassifier.TextClassifierEvent.LanguageDetectionEvent
          languageDetectionEvent) {
    LanguageDetectionEvent.Builder builder =
        new LanguageDetectionEvent.Builder(languageDetectionEvent.getEventType());
    copyCommonFields(languageDetectionEvent, builder);
    return builder.build();
  }

  @Nullable
  private static TextClassificationContext fromPlatform(
      @Nullable android.view.textclassifier.TextClassificationContext textClassificationContext) {
    if (textClassificationContext == null) {
      return null;
    }
    return new TextClassificationContext.Builder(
            textClassificationContext.getPackageName(), textClassificationContext.getWidgetType())
        .setWidgetVersion(textClassificationContext.getWidgetVersion())
        .build();
  }

  private static void copyCommonFields(
      android.view.textclassifier.TextClassifierEvent sourceEvent,
      TextClassifierEvent.Builder<?> destBuilder) {
    destBuilder
        .setActionIndices(sourceEvent.getActionIndices())
        .setEventContext(fromPlatform(sourceEvent.getEventContext()))
        .setEntityTypes(sourceEvent.getEntityTypes())
        .setEventIndex(sourceEvent.getEventIndex())
        .setExtras(sourceEvent.getExtras())
        .setLocale(sourceEvent.getLocale() == null ? null : sourceEvent.getLocale().toLocale())
        .setModelName(sourceEvent.getModelName())
        .setResultId(sourceEvent.getResultId())
        .setScores(sourceEvent.getScores());
  }

  private TextClassifierEventConverter() {}
}
