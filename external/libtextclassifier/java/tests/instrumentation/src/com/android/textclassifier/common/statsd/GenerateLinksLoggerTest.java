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
import android.view.textclassifier.TextLinks;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.TextLinkifyEvent;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.google.common.base.Optional;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import java.util.Locale;
import java.util.Map;
import java.util.stream.Collectors;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@LargeTest
public class GenerateLinksLoggerTest {
  private static final String PACKAGE_NAME = "package.name";
  private static final int LATENCY_MS = 123;
  /** A statsd config ID, which is arbitrary. */
  private static final long CONFIG_ID = 689777;

  private static final ModelInfo ANNOTATOR_MODEL =
      new ModelInfo(1, ImmutableList.of(Locale.ENGLISH));
  private static final ModelInfo LANGID_MODEL =
      new ModelInfo(2, ImmutableList.of(Locale.forLanguageTag("*")));

  @Before
  public void setup() throws Exception {
    StatsdTestUtils.cleanup(CONFIG_ID);

    StatsdConfig.Builder builder =
        StatsdConfig.newBuilder()
            .setId(CONFIG_ID)
            .addAllowedLogSource(ApplicationProvider.getApplicationContext().getPackageName());
    StatsdTestUtils.addAtomMatcher(builder, Atom.TEXT_LINKIFY_EVENT_FIELD_NUMBER);
    StatsdTestUtils.pushConfig(builder.build());
  }

  @After
  public void tearDown() throws Exception {
    StatsdTestUtils.cleanup(CONFIG_ID);
  }

  @Test
  public void logGenerateLinks_allFieldsAreSet() throws Exception {
    String phoneText = "+12122537077";
    String testText = "The number is " + phoneText;
    int phoneOffset = testText.indexOf(phoneText);
    Map<String, Float> phoneEntityScores = ImmutableMap.of(TextClassifier.TYPE_PHONE, 1.0f);
    TextLinks links =
        new TextLinks.Builder(testText)
            .addLink(phoneOffset, phoneOffset + phoneText.length(), phoneEntityScores)
            .build();
    String uuid = "uuid";

    GenerateLinksLogger generateLinksLogger =
        new GenerateLinksLogger(/* sampleRate= */ 1, () -> uuid);
    generateLinksLogger.logGenerateLinks(
        testText,
        links,
        PACKAGE_NAME,
        LATENCY_MS,
        Optional.of(ANNOTATOR_MODEL),
        Optional.of(LANGID_MODEL));
    ImmutableList<Atom> loggedAtoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);

    ImmutableList<TextLinkifyEvent> loggedEvents =
        ImmutableList.copyOf(
            loggedAtoms.stream().map(Atom::getTextLinkifyEvent).collect(Collectors.toList()));

    assertThat(loggedEvents).hasSize(2);
    TextLinkifyEvent summaryEvent =
        AtomsProto.TextLinkifyEvent.newBuilder()
            .setSessionId(uuid)
            .setEventIndex(0)
            .setModelName("en_v1")
            .setWidgetType(WidgetType.WIDGET_TYPE_UNKNOWN)
            .setEventType(EventType.LINKS_GENERATED)
            .setPackageName(PACKAGE_NAME)
            .setEntityType("")
            .setNumLinks(1)
            .setTextLength(testText.length())
            .setLinkedTextLength(phoneText.length())
            .setLatencyMillis(LATENCY_MS)
            .setLangidModelName("und_v2")
            .build();
    TextLinkifyEvent phoneEvent =
        AtomsProto.TextLinkifyEvent.newBuilder()
            .setSessionId(uuid)
            .setEventIndex(0)
            .setModelName("en_v1")
            .setWidgetType(WidgetType.WIDGET_TYPE_UNKNOWN)
            .setEventType(EventType.LINKS_GENERATED)
            .setPackageName(PACKAGE_NAME)
            .setEntityType(TextClassifier.TYPE_PHONE)
            .setNumLinks(1)
            .setTextLength(testText.length())
            .setLinkedTextLength(phoneText.length())
            .setLatencyMillis(LATENCY_MS)
            .setLangidModelName("und_v2")
            .build();
    assertThat(loggedEvents).containsExactly(summaryEvent, phoneEvent).inOrder();
  }

  @Test
  public void logGenerateLinks_multipleLinks() throws Exception {
    String phoneText = "+12122537077";
    String addressText = "1600 Amphitheater Parkway, Mountain View, CA";
    String testText = "The number is " + phoneText + ", the address is " + addressText;
    int phoneOffset = testText.indexOf(phoneText);
    int addressOffset = testText.indexOf(addressText);
    Map<String, Float> phoneEntityScores = ImmutableMap.of(TextClassifier.TYPE_PHONE, 1.0f);
    Map<String, Float> addressEntityScores = ImmutableMap.of(TextClassifier.TYPE_ADDRESS, 1.0f);
    TextLinks links =
        new TextLinks.Builder(testText)
            .addLink(phoneOffset, phoneOffset + phoneText.length(), phoneEntityScores)
            .addLink(addressOffset, addressOffset + addressText.length(), addressEntityScores)
            .build();
    String uuid = "uuid";

    GenerateLinksLogger generateLinksLogger =
        new GenerateLinksLogger(/* sampleRate= */ 1, () -> uuid);
    generateLinksLogger.logGenerateLinks(
        testText,
        links,
        PACKAGE_NAME,
        LATENCY_MS,
        Optional.of(ANNOTATOR_MODEL),
        Optional.of(LANGID_MODEL));
    ImmutableList<Atom> loggedAtoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID);

    ImmutableList<TextLinkifyEvent> loggedEvents =
        ImmutableList.copyOf(
            loggedAtoms.stream().map(Atom::getTextLinkifyEvent).collect(Collectors.toList()));
    assertThat(loggedEvents).hasSize(3);

    TextLinkifyEvent summaryEvent = loggedEvents.get(0);
    assertThat(summaryEvent.getEntityType()).isEmpty();
    assertThat(summaryEvent.getNumLinks()).isEqualTo(2);
    assertThat(summaryEvent.getLinkedTextLength())
        .isEqualTo(phoneText.length() + addressText.length());

    TextLinkifyEvent addressEvent = loggedEvents.get(1);
    assertThat(addressEvent.getEntityType()).isEqualTo(TextClassifier.TYPE_ADDRESS);
    assertThat(addressEvent.getNumLinks()).isEqualTo(1);
    assertThat(addressEvent.getLinkedTextLength()).isEqualTo(addressText.length());

    TextLinkifyEvent phoneEvent = loggedEvents.get(2);
    assertThat(phoneEvent.getEntityType()).isEqualTo(TextClassifier.TYPE_PHONE);
    assertThat(phoneEvent.getNumLinks()).isEqualTo(1);
    assertThat(phoneEvent.getLinkedTextLength()).isEqualTo(phoneText.length());
  }
}
