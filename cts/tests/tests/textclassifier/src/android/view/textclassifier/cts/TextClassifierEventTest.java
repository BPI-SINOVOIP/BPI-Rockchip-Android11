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

package android.view.textclassifier.cts;

import static com.google.common.truth.Truth.assertThat;

import android.icu.util.ULocale;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassifierEventTest {
    private static final float TOLERANCE = 0.000001f;
    private static final String MODEL_NAME = "model_name";

    @Test
    public void testTextSelectionEvent_minimal() {
        final TextClassifierEvent.TextSelectionEvent event = createMinimalTextSelectionEvent();

        TextClassifierEvent.TextSelectionEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.TextSelectionEvent.CREATOR);

        assertMinimalTextSelectionEvent(event);
        assertMinimalTextSelectionEvent(result);
    }

    @Test
    public void testTextSelectionEvent_full() {
        final TextClassifierEvent.TextSelectionEvent event = createFullTextSelectionEvent();

        TextClassifierEvent.TextSelectionEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.TextSelectionEvent.CREATOR);

        assertFullTextSelectionEvent(event);
        assertFullTextSelectionEvent(result);
    }

    @Test
    public void testTextLinkifyEvent_minimal() {
        final TextClassifierEvent.TextLinkifyEvent event = createMinimalTextLinkifyEvent();

        TextClassifierEvent.TextLinkifyEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.TextLinkifyEvent.CREATOR);

        assertMinimalTextLinkifyEvent(event);
        assertMinimalTextLinkifyEvent(result);
    }

    @Test
    public void testTextLinkifyEvent_full() {
        final TextClassifierEvent.TextLinkifyEvent event = createFullTextLinkifyEvent();

        TextClassifierEvent.TextLinkifyEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.TextLinkifyEvent.CREATOR);

        assertFullTextLinkifyEvent(event);
        assertFullTextLinkifyEvent(result);
    }

    @Test
    public void testConversationActionsEvent_minimal() {
        final TextClassifierEvent.ConversationActionsEvent event =
                createMinimalConversationActionsEvent();

        TextClassifierEvent.ConversationActionsEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.ConversationActionsEvent.CREATOR);

        assertMinimalConversationActionsEvent(event);
        assertMinimalConversationActionsEvent(result);
    }

    @Test
    public void testConversationActionsEvent_full() {
        final TextClassifierEvent.ConversationActionsEvent event =
                createFullConversationActionsEvent();

        TextClassifierEvent.ConversationActionsEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.ConversationActionsEvent.CREATOR);

        assertFullConversationActionsEvent(event);
        assertFullConversationActionsEvent(result);
    }

    @Test
    public void testLanguageDetectionEvent_minimal() {
        final TextClassifierEvent.LanguageDetectionEvent event =
                createMinimalLanguageDetectionEvent();

        TextClassifierEvent.LanguageDetectionEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.LanguageDetectionEvent.CREATOR);

        assertMinimalLanguageDetectionEvent(event);
        assertMinimalLanguageDetectionEvent(result);
    }

    @Test
    public void testLanguageDetectionEvent_full() {
        final TextClassifierEvent.LanguageDetectionEvent event =
                createFullLanguageDetectionEvent();

        TextClassifierEvent.LanguageDetectionEvent result =
                parcelizeDeparcelize(event, TextClassifierEvent.LanguageDetectionEvent.CREATOR);

        assertFullLanguageDetectionEvent(event);
        assertFullLanguageDetectionEvent(result);
    }

    private TextClassifierEvent.TextSelectionEvent createMinimalTextSelectionEvent() {
        return new TextClassifierEvent.TextSelectionEvent.Builder(
                TextClassifierEvent.TYPE_SELECTION_DESTROYED)
                .build();
    }

    private void assertMinimalTextSelectionEvent(TextClassifierEvent.TextSelectionEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_SELECTION);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_SELECTION_DESTROYED);
        assertThat(event.getEventIndex()).isEqualTo(0);
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getRelativeWordStartIndex()).isEqualTo(0);
        assertThat(event.getRelativeWordEndIndex()).isEqualTo(0);
        assertThat(event.getRelativeSuggestedWordStartIndex()).isEqualTo(0);
        assertThat(event.getRelativeSuggestedWordEndIndex()).isEqualTo(0);
        assertThat(event.getResultId()).isNull();
        assertThat(event.getActionIndices()).isEmpty();
        assertThat(event.getExtras().size()).isEqualTo(0);
        assertThat(event.getEventContext()).isNull();
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getLocale()).isNull();
        assertThat(event.getModelName()).isNull();
    }

    private TextClassifierEvent.TextSelectionEvent createFullTextSelectionEvent() {
        final Bundle extra = new Bundle();
        extra.putString("key", "value");
        return new TextClassifierEvent.TextSelectionEvent.Builder(
                TextClassifierEvent.TYPE_SELECTION_STARTED)
                .setEventIndex(2)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS)
                .setRelativeWordStartIndex(1)
                .setRelativeWordEndIndex(2)
                .setRelativeSuggestedWordStartIndex(-1)
                .setRelativeSuggestedWordEndIndex(3)
                .setResultId("androidtc-en-v606-1234")
                .setActionIndices(1, 2, 5)
                .setExtras(extra)
                .setEventContext(new TextClassificationContext.Builder(
                        "pkg", TextClassifier.WIDGET_TYPE_TEXTVIEW)
                        .setWidgetVersion(TextView.class.getName())
                        .build())
                .setScores(0.5f)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE)
                .setLocale(ULocale.US)
                .setModelName(MODEL_NAME)
                .build();
    }

    private void assertFullTextSelectionEvent(TextClassifierEvent.TextSelectionEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_SELECTION);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_SELECTION_STARTED);
        assertThat(event.getEventIndex()).isEqualTo(2);
        assertThat(event.getEntityTypes()).asList()
                .containsExactly(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE);
        assertThat(event.getRelativeWordStartIndex()).isEqualTo(1);
        assertThat(event.getRelativeWordEndIndex()).isEqualTo(2);
        assertThat(event.getRelativeSuggestedWordStartIndex()).isEqualTo(-1);
        assertThat(event.getRelativeSuggestedWordEndIndex()).isEqualTo(3);
        assertThat(event.getResultId()).isEqualTo("androidtc-en-v606-1234");
        assertThat(event.getActionIndices()).asList().containsExactly(1, 2, 5);
        assertThat(event.getExtras().get("key")).isEqualTo("value");
        assertThat(event.getEventContext().getPackageName()).isEqualTo("pkg");
        assertThat(event.getEventContext().getWidgetType())
                .isEqualTo(TextClassifier.WIDGET_TYPE_TEXTVIEW);
        assertThat(event.getEventContext().getWidgetVersion()).isEqualTo(TextView.class.getName());
        assertThat(event.getScores()).hasLength(1);
        assertThat(event.getScores()[0]).isWithin(TOLERANCE).of(0.5f);
        assertThat(event.getLocale()).isEqualTo(ULocale.US);
        assertThat(event.getModelName()).isEqualTo(MODEL_NAME);
    }

    private TextClassifierEvent.TextLinkifyEvent createMinimalTextLinkifyEvent() {
        return new TextClassifierEvent.TextLinkifyEvent.Builder(
                TextClassifierEvent.TYPE_LINK_CLICKED)
                .build();
    }

    private void assertMinimalTextLinkifyEvent(TextClassifierEvent.TextLinkifyEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LINKIFY);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_LINK_CLICKED);
        assertThat(event.getEventIndex()).isEqualTo(0);
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getResultId()).isNull();
        assertThat(event.getActionIndices()).isEmpty();
        assertThat(event.getExtras().size()).isEqualTo(0);
        assertThat(event.getEventContext()).isNull();
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getLocale()).isNull();
        assertThat(event.getModelName()).isNull();
    }

    private TextClassifierEvent.TextLinkifyEvent createFullTextLinkifyEvent() {
        final Bundle extra = new Bundle();
        extra.putString("key", "value");
        return new TextClassifierEvent.TextLinkifyEvent.Builder(
                TextClassifierEvent.TYPE_LINK_CLICKED)
                .setEventIndex(2)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS)
                .setResultId("androidtc-en-v606-1234")
                .setActionIndices(1, 2, 5)
                .setExtras(extra)
                .setEventContext(new TextClassificationContext.Builder(
                        "pkg", TextClassifier.WIDGET_TYPE_TEXTVIEW)
                        .setWidgetVersion(TextView.class.getName())
                        .build())
                .setScores(0.5f)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE)
                .setLocale(ULocale.US)
                .setModelName(MODEL_NAME)
                .build();
    }

    private void assertFullTextLinkifyEvent(TextClassifierEvent.TextLinkifyEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(TextClassifierEvent.CATEGORY_LINKIFY);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_LINK_CLICKED);
        assertThat(event.getEventIndex()).isEqualTo(2);
        assertThat(event.getEntityTypes()).asList()
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
        assertThat(event.getLocale()).isEqualTo(ULocale.US);
        assertThat(event.getModelName()).isEqualTo(MODEL_NAME);
    }

    private TextClassifierEvent.ConversationActionsEvent createMinimalConversationActionsEvent() {
        return new TextClassifierEvent.ConversationActionsEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
                .build();
    }

    private void assertMinimalConversationActionsEvent(
            TextClassifierEvent.ConversationActionsEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(
                TextClassifierEvent.CATEGORY_CONVERSATION_ACTIONS);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
        assertThat(event.getEventIndex()).isEqualTo(0);
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getResultId()).isNull();
        assertThat(event.getActionIndices()).isEmpty();
        assertThat(event.getExtras().size()).isEqualTo(0);
        assertThat(event.getEventContext()).isNull();
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getLocale()).isNull();
        assertThat(event.getModelName()).isNull();
    }

    private TextClassifierEvent.ConversationActionsEvent createFullConversationActionsEvent() {
        final Bundle extra = new Bundle();
        extra.putString("key", "value");
        return new TextClassifierEvent.ConversationActionsEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
                .setEventIndex(2)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS)
                .setResultId("androidtc-en-v606-1234")
                .setActionIndices(1, 2, 5)
                .setExtras(extra)
                .setEventContext(new TextClassificationContext.Builder(
                        "pkg", TextClassifier.WIDGET_TYPE_TEXTVIEW)
                        .setWidgetVersion(TextView.class.getName())
                        .build())
                .setScores(0.5f)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE)
                .setLocale(ULocale.US)
                .setModelName(MODEL_NAME)
                .build();
    }

    private void assertFullConversationActionsEvent(
            TextClassifierEvent.ConversationActionsEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(
                TextClassifierEvent.CATEGORY_CONVERSATION_ACTIONS);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
        assertThat(event.getEventIndex()).isEqualTo(2);
        assertThat(event.getEntityTypes()).asList()
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
        assertThat(event.getLocale()).isEqualTo(ULocale.US);
        assertThat(event.getModelName()).isEqualTo(MODEL_NAME);
    }

    private TextClassifierEvent.LanguageDetectionEvent createMinimalLanguageDetectionEvent() {
        return new TextClassifierEvent.LanguageDetectionEvent.Builder(
                TextClassifierEvent.TYPE_SMART_ACTION)
                .build();
    }

    private void assertMinimalLanguageDetectionEvent(
            TextClassifierEvent.LanguageDetectionEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(
                TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_SMART_ACTION);
        assertThat(event.getEventIndex()).isEqualTo(0);
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getResultId()).isNull();
        assertThat(event.getActionIndices()).isEmpty();
        assertThat(event.getExtras().size()).isEqualTo(0);
        assertThat(event.getEventContext()).isNull();
        assertThat(event.getEntityTypes()).isEmpty();
        assertThat(event.getLocale()).isNull();
        assertThat(event.getModelName()).isNull();
    }

    private TextClassifierEvent.LanguageDetectionEvent createFullLanguageDetectionEvent() {
        final Bundle extra = new Bundle();
        extra.putString("key", "value");
        return new TextClassifierEvent.LanguageDetectionEvent.Builder(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN)
                .setEventIndex(2)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS)
                .setResultId("androidtc-en-v606-1234")
                .setActionIndices(1, 2, 5)
                .setExtras(extra)
                .setEventContext(new TextClassificationContext.Builder(
                        "pkg", TextClassifier.WIDGET_TYPE_TEXTVIEW)
                        .setWidgetVersion(TextView.class.getName())
                        .build())
                .setScores(0.5f)
                .setEntityTypes(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE)
                .setLocale(ULocale.US)
                .setModelName(MODEL_NAME)
                .build();
    }

    private void assertFullLanguageDetectionEvent(
            TextClassifierEvent.LanguageDetectionEvent event) {
        assertThat(event.getEventCategory()).isEqualTo(
                TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION);
        assertThat(event.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
        assertThat(event.getEventIndex()).isEqualTo(2);
        assertThat(event.getEntityTypes()).asList()
                .containsExactly(TextClassifier.TYPE_ADDRESS, TextClassifier.TYPE_DATE);
        assertThat(event.getResultId()).isEqualTo("androidtc-en-v606-1234");
        assertThat(event.getActionIndices()).asList().containsExactly(1, 2, 5);
        assertThat(event.getLocale()).isEqualTo(ULocale.US);
        assertThat(event.getModelName()).isEqualTo(MODEL_NAME);
        assertThat(event.getExtras().get("key")).isEqualTo("value");
        assertThat(event.getEventContext().getPackageName()).isEqualTo("pkg");
        assertThat(event.getEventContext().getWidgetType())
                .isEqualTo(TextClassifier.WIDGET_TYPE_TEXTVIEW);
        assertThat(event.getEventContext().getWidgetVersion()).isEqualTo(TextView.class.getName());
        assertThat(event.getScores()).hasLength(1);
        assertThat(event.getScores()[0]).isWithin(TOLERANCE).of(0.5f);
    }

    private <T extends Parcelable> T parcelizeDeparcelize(
            T parcelable, Parcelable.Creator<T> creator) {
        Parcel parcel = Parcel.obtain();
        parcelable.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        return creator.createFromParcel(parcel);
    }
}
