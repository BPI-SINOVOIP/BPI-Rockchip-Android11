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

import android.stats.textclassifier.EventType;
import android.stats.textclassifier.WidgetType;
import android.view.textclassifier.TextClassifier;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto;
import com.android.os.AtomsProto.Atom;
import com.android.textclassifier.common.logging.TextClassificationContext;
import com.android.textclassifier.common.logging.TextClassificationSessionId;
import com.android.textclassifier.common.logging.TextClassifierEvent;
import com.google.common.collect.ImmutableList;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@LargeTest
public class TextClassifierEventLoggerTest {
  private static final String PKG_NAME = "pkg.name";
  private static final String WIDGET_TYPE = TextClassifier.WIDGET_TYPE_WEBVIEW;
  private static final String MODEL_NAME = "model_name";
  /** A statsd config ID, which is arbitrary. */
  private static final long CONFIG_ID = 689777;

  private TextClassifierEventLogger textClassifierEventLogger;

  @Before
  public void setup() throws Exception {
    StatsdTestUtils.cleanup(CONFIG_ID);

    StatsdConfig.Builder builder =
        StatsdConfig.newBuilder()
            .setId(CONFIG_ID)
            .addAllowedLogSource(ApplicationProvider.getApplicationContext().getPackageName());
    StatsdTestUtils.addAtomMatcher(builder, Atom.TEXT_SELECTION_EVENT_FIELD_NUMBER);
    StatsdTestUtils.addAtomMatcher(builder, Atom.TEXT_LINKIFY_EVENT_FIELD_NUMBER);
    StatsdTestUtils.addAtomMatcher(builder, Atom.CONVERSATION_ACTIONS_EVENT_FIELD_NUMBER);
    StatsdTestUtils.addAtomMatcher(builder, Atom.LANGUAGE_DETECTION_EVENT_FIELD_NUMBER);
    StatsdTestUtils.pushConfig(builder.build());

    textClassifierEventLogger = new TextClassifierEventLogger();
  }

  @After
  public void tearDown() throws Exception {
    StatsdTestUtils.cleanup(CONFIG_ID);
  }

  @Test
  public void writeEvent_textSelectionEvent() throws Exception {
    TextClassificationSessionId sessionId = new TextClassificationSessionId();
    TextClassifierEvent.TextSelectionEvent textSelectionEvent =
        new TextClassifierEvent.TextSelectionEvent.Builder(
                TextClassifierEvent.TYPE_SELECTION_STARTED)
            .setEventContext(createTextClassificationContext())
            .setResultId("androidtc|en_v705;und_v1|12345")
            .setEventIndex(1)
            .setEntityTypes(TextClassifier.TYPE_ADDRESS)
            .setRelativeWordStartIndex(2)
            .setRelativeWordEndIndex(3)
            .setRelativeSuggestedWordStartIndex(1)
            .setRelativeSuggestedWordEndIndex(4)
            .build();

    textClassifierEventLogger.writeEvent(sessionId, textSelectionEvent);

    AtomsProto.TextSelectionEvent event =
        AtomsProto.TextSelectionEvent.newBuilder()
            .setSessionId(sessionId.getValue())
            .setEventType(EventType.SELECTION_STARTED)
            .setModelName("en_v705")
            .setWidgetType(WidgetType.WIDGET_TYPE_WEBVIEW)
            .setEventIndex(1)
            .setEntityType(TextClassifier.TYPE_ADDRESS)
            .setRelativeWordStartIndex(2)
            .setRelativeWordEndIndex(3)
            .setRelativeSuggestedWordStartIndex(1)
            .setRelativeSuggestedWordEndIndex(4)
            .setPackageName(PKG_NAME)
            .setLangidModelName("und_v1")
            .build();
    ImmutableList<Atom> atoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);
    assertThat(atoms).hasSize(1);
    assertThat(atoms.get(0).getTextSelectionEvent()).isEqualTo(event);
  }

  @Test
  public void writeEvent_textSelectionEvent_autoToSingle() throws Exception {
    TextClassificationSessionId sessionId = new TextClassificationSessionId();
    TextClassifierEvent.TextSelectionEvent textSelectionEvent =
        new TextClassifierEvent.TextSelectionEvent.Builder(TextClassifierEvent.TYPE_AUTO_SELECTION)
            .setResultId("androidtc|en_v705;und_v1|12345")
            .setRelativeWordStartIndex(2)
            .setRelativeWordEndIndex(3)
            .build();

    textClassifierEventLogger.writeEvent(sessionId, textSelectionEvent);

    ImmutableList<Atom> atoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);
    assertThat(atoms).hasSize(1);
    assertThat(atoms.get(0).getTextSelectionEvent().getEventType())
        .isEqualTo(EventType.SMART_SELECTION_SINGLE);
  }

  @Test
  public void writeEvent_textSelectionEvent_autoToMulti() throws Exception {
    TextClassificationSessionId sessionId = new TextClassificationSessionId();
    TextClassifierEvent.TextSelectionEvent textSelectionEvent =
        new TextClassifierEvent.TextSelectionEvent.Builder(TextClassifierEvent.TYPE_AUTO_SELECTION)
            .setResultId("androidtc|en_v705;und_v1|12345")
            .setRelativeWordStartIndex(2)
            .setRelativeWordEndIndex(4)
            .build();

    textClassifierEventLogger.writeEvent(sessionId, textSelectionEvent);

    ImmutableList<Atom> atoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);
    assertThat(atoms).hasSize(1);
    assertThat(atoms.get(0).getTextSelectionEvent().getEventType())
        .isEqualTo(EventType.SMART_SELECTION_MULTI);
  }

  @Test
  public void writeEvent_textSelectionEvent_keepAuto() throws Exception {
    TextClassificationSessionId sessionId = new TextClassificationSessionId();
    TextClassifierEvent.TextSelectionEvent textSelectionEvent =
        new TextClassifierEvent.TextSelectionEvent.Builder(TextClassifierEvent.TYPE_AUTO_SELECTION)
            .setResultId("aiai|en_v705;und_v1|12345")
            .setRelativeWordStartIndex(2)
            .setRelativeWordEndIndex(4)
            .build();

    textClassifierEventLogger.writeEvent(sessionId, textSelectionEvent);

    ImmutableList<Atom> atoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);
    assertThat(atoms).hasSize(1);
    assertThat(atoms.get(0).getTextSelectionEvent().getEventType())
        .isEqualTo(EventType.AUTO_SELECTION);
  }

  @Test
  public void writeEvent_textLinkifyEvent() throws Exception {
    TextClassificationSessionId sessionId = new TextClassificationSessionId();
    TextClassifierEvent.TextLinkifyEvent textLinkifyEvent =
        new TextClassifierEvent.TextLinkifyEvent.Builder(TextClassifierEvent.TYPE_SELECTION_STARTED)
            .setEventContext(createTextClassificationContext())
            .setResultId("androidtc|en_v705;und_v1|12345")
            .setEventIndex(1)
            .setEntityTypes(TextClassifier.TYPE_ADDRESS)
            .build();

    textClassifierEventLogger.writeEvent(sessionId, textLinkifyEvent);

    AtomsProto.TextLinkifyEvent event =
        AtomsProto.TextLinkifyEvent.newBuilder()
            .setSessionId(sessionId.getValue())
            .setEventType(EventType.SELECTION_STARTED)
            .setModelName("en_v705")
            .setWidgetType(WidgetType.WIDGET_TYPE_WEBVIEW)
            .setEventIndex(1)
            .setEntityType(TextClassifier.TYPE_ADDRESS)
            .setNumLinks(0)
            .setLinkedTextLength(0)
            .setTextLength(0)
            .setLatencyMillis(0)
            .setPackageName(PKG_NAME)
            .setLangidModelName("und_v1")
            .build();
    ImmutableList<Atom> atoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);
    assertThat(atoms).hasSize(1);
    assertThat(atoms.get(0).getTextLinkifyEvent()).isEqualTo(event);
  }

  @Test
  public void writeEvent_textConversationActionEvent() throws Exception {
    TextClassificationSessionId sessionId = new TextClassificationSessionId();
    TextClassifierEvent.ConversationActionsEvent conversationActionsEvent =
        new TextClassifierEvent.ConversationActionsEvent.Builder(
                TextClassifierEvent.TYPE_SELECTION_STARTED)
            .setEventContext(createTextClassificationContext())
            .setResultId("android_tc|en_v1;zh_v2;und_v3|12345")
            .setEventIndex(1)
            .setEntityTypes("first", "second", "third", "fourth")
            .setScores(0.5f)
            .build();

    textClassifierEventLogger.writeEvent(sessionId, conversationActionsEvent);

    AtomsProto.ConversationActionsEvent event =
        AtomsProto.ConversationActionsEvent.newBuilder()
            .setSessionId(sessionId.getValue())
            .setEventType(EventType.SELECTION_STARTED)
            .setModelName("en_v1")
            .setWidgetType(WidgetType.WIDGET_TYPE_WEBVIEW)
            .setFirstEntityType("first")
            .setSecondEntityType("second")
            .setThirdEntityType("third")
            .setScore(0.5f)
            .setPackageName(PKG_NAME)
            .setAnnotatorModelName("zh_v2")
            .setLangidModelName("und_v3")
            .build();
    ImmutableList<Atom> atoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);
    assertThat(atoms).hasSize(1);
    assertThat(atoms.get(0).getConversationActionsEvent()).isEqualTo(event);
  }

  @Test
  public void writeEvent_languageDetectionEvent() throws Exception {
    TextClassificationSessionId sessionId = new TextClassificationSessionId();
    TextClassifierEvent.LanguageDetectionEvent languageDetectionEvent =
        new TextClassifierEvent.LanguageDetectionEvent.Builder(
                TextClassifierEvent.TYPE_SELECTION_STARTED)
            .setEventContext(createTextClassificationContext())
            .setModelName(MODEL_NAME)
            .setEventIndex(1)
            .setEntityTypes("en")
            .setScores(0.5f)
            .setActionIndices(1)
            .build();

    textClassifierEventLogger.writeEvent(sessionId, languageDetectionEvent);
    AtomsProto.LanguageDetectionEvent event =
        AtomsProto.LanguageDetectionEvent.newBuilder()
            .setSessionId(sessionId.getValue())
            .setEventType(EventType.SELECTION_STARTED)
            .setModelName(MODEL_NAME)
            .setWidgetType(WidgetType.WIDGET_TYPE_WEBVIEW)
            .setLanguageTag("en")
            .setScore(0.5f)
            .setActionIndex(1)
            .setPackageName(PKG_NAME)
            .build();
    ImmutableList<Atom> atoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);
    assertThat(atoms).hasSize(1);
    assertThat(atoms.get(0).getLanguageDetectionEvent()).isEqualTo(event);
  }

  private static TextClassificationContext createTextClassificationContext() {
    return new TextClassificationContext.Builder(PKG_NAME, WIDGET_TYPE).build();
  }
}
