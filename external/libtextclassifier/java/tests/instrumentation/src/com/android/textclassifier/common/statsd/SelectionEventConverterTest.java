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

import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import android.view.textclassifier.TextSelection;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.common.logging.ResultIdUtils;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.google.common.base.Optional;
import com.google.common.collect.ImmutableList;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Locale;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class SelectionEventConverterTest {
  private static final String PKG_NAME = "com.pkg";
  private static final String WIDGET_TYPE = TextClassifier.WIDGET_TYPE_EDITTEXT;
  private static final int START = 2;
  private static final int SMART_START = 1;
  private static final int SMART_END = 3;
  private TestTextClassifier testTextClassifier;
  private TextClassifier session;

  @Before
  public void setup() {
    TextClassificationManager textClassificationManager =
        ApplicationProvider.getApplicationContext()
            .getSystemService(TextClassificationManager.class);
    testTextClassifier = new TestTextClassifier();
    textClassificationManager.setTextClassifier(testTextClassifier);
    session = textClassificationManager.createTextClassificationSession(createEventContext());
  }

  @Test
  public void convert_started() {
    session.onSelectionEvent(
        SelectionEvent.createSelectionStartedEvent(SelectionEvent.INVOCATION_MANUAL, START));

    SelectionEvent interceptedEvent = testTextClassifier.popLastSelectionEvent();
    TextClassifierEvent textClassifierEvent =
        SelectionEventConverter.toTextClassifierEvent(interceptedEvent);

    assertEventContext(textClassifierEvent.getEventContext());
    assertThat(textClassifierEvent.getEventIndex()).isEqualTo(0);
    assertThat(textClassifierEvent.getEventType())
        .isEqualTo(TextClassifierEvent.TYPE_SELECTION_STARTED);
  }

  @Test
  public void convert_smartSelection() {
    session.onSelectionEvent(
        SelectionEvent.createSelectionStartedEvent(SelectionEvent.INVOCATION_MANUAL, START));
    String resultId = createResultId();
    session.onSelectionEvent(
        SelectionEvent.createSelectionActionEvent(
            SMART_START,
            SMART_END,
            SelectionEvent.ACTION_SMART_SHARE,
            new TextClassification.Builder()
                .setEntityType(TextClassifier.TYPE_ADDRESS, 1.0f)
                .setId(resultId)
                .build()));

    SelectionEvent interceptedEvent = testTextClassifier.popLastSelectionEvent();
    TextClassifierEvent.TextSelectionEvent textSelectionEvent =
        (TextClassifierEvent.TextSelectionEvent)
            SelectionEventConverter.toTextClassifierEvent(interceptedEvent);

    assertEventContext(textSelectionEvent.getEventContext());
    assertThat(textSelectionEvent.getRelativeWordStartIndex()).isEqualTo(-1);
    assertThat(textSelectionEvent.getRelativeWordEndIndex()).isEqualTo(1);
    assertThat(textSelectionEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_SMART_ACTION);
    assertThat(textSelectionEvent.getEventIndex()).isEqualTo(1);
    assertThat(textSelectionEvent.getEntityTypes())
        .asList()
        .containsExactly(TextClassifier.TYPE_ADDRESS);
    assertThat(textSelectionEvent.getResultId()).isEqualTo(resultId);
  }

  @Test
  public void convert_smartShare() {
    session.onSelectionEvent(
        SelectionEvent.createSelectionStartedEvent(SelectionEvent.INVOCATION_MANUAL, START));
    String resultId = createResultId();
    session.onSelectionEvent(
        SelectionEvent.createSelectionModifiedEvent(
            SMART_START,
            SMART_END,
            new TextSelection.Builder(SMART_START, SMART_END)
                .setEntityType(TextClassifier.TYPE_ADDRESS, 1.0f)
                .setId(resultId)
                .build()));

    SelectionEvent interceptedEvent = testTextClassifier.popLastSelectionEvent();
    TextClassifierEvent.TextSelectionEvent textSelectionEvent =
        (TextClassifierEvent.TextSelectionEvent)
            SelectionEventConverter.toTextClassifierEvent(interceptedEvent);

    assertEventContext(textSelectionEvent.getEventContext());
    assertThat(textSelectionEvent.getRelativeSuggestedWordStartIndex()).isEqualTo(-1);
    assertThat(textSelectionEvent.getRelativeSuggestedWordEndIndex()).isEqualTo(1);
    assertThat(textSelectionEvent.getEventType())
        .isEqualTo(TextClassifierEvent.TYPE_SMART_SELECTION_MULTI);
    assertThat(textSelectionEvent.getEventIndex()).isEqualTo(1);
    assertThat(textSelectionEvent.getEntityTypes())
        .asList()
        .containsExactly(TextClassifier.TYPE_ADDRESS);
    assertThat(textSelectionEvent.getResultId()).isEqualTo(resultId);
  }

  @Test
  public void convert_smartLinkify() {
    session.onSelectionEvent(
        SelectionEvent.createSelectionStartedEvent(SelectionEvent.INVOCATION_LINK, START));
    String resultId = createResultId();
    session.onSelectionEvent(
        SelectionEvent.createSelectionModifiedEvent(
            SMART_START,
            SMART_END,
            new TextSelection.Builder(SMART_START, SMART_END)
                .setEntityType(TextClassifier.TYPE_ADDRESS, 1.0f)
                .setId(resultId)
                .build()));

    SelectionEvent interceptedEvent = testTextClassifier.popLastSelectionEvent();
    TextClassifierEvent.TextLinkifyEvent textLinkifyEvent =
        (TextClassifierEvent.TextLinkifyEvent)
            SelectionEventConverter.toTextClassifierEvent(interceptedEvent);

    assertEventContext(textLinkifyEvent.getEventContext());
    assertThat(textLinkifyEvent.getEventType())
        .isEqualTo(TextClassifierEvent.TYPE_SMART_SELECTION_MULTI);
    assertThat(textLinkifyEvent.getEventIndex()).isEqualTo(1);
    assertThat(textLinkifyEvent.getEntityTypes())
        .asList()
        .containsExactly(TextClassifier.TYPE_ADDRESS);
    assertThat(textLinkifyEvent.getResultId()).isEqualTo(resultId);
  }

  private static TextClassificationContext createEventContext() {
    return new TextClassificationContext.Builder(PKG_NAME, TextClassifier.WIDGET_TYPE_EDITTEXT)
        .build();
  }

  private static void assertEventContext(TextClassificationContext eventContext) {
    assertThat(eventContext.getPackageName()).isEqualTo(PKG_NAME);
    assertThat(eventContext.getWidgetType()).isEqualTo(WIDGET_TYPE);
  }

  private static String createResultId() {
    return ResultIdUtils.createId(
        /*hash=*/ 12345,
        ImmutableList.of(
            Optional.of(new ModelInfo(/* version= */ 702, ImmutableList.of(Locale.ENGLISH)))));
  }

  private static class TestTextClassifier implements TextClassifier {
    private final Deque<SelectionEvent> selectionEvents = new ArrayDeque<>();

    @Override
    public void onSelectionEvent(SelectionEvent event) {
      selectionEvents.push(event);
    }

    SelectionEvent popLastSelectionEvent() {
      return selectionEvents.pop();
    }
  }
}
