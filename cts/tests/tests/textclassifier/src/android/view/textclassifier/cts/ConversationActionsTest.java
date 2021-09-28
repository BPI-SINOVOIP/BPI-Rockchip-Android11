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

import android.app.PendingIntent;
import android.app.Person;
import android.app.RemoteAction;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.ConversationActions;
import android.view.textclassifier.TextClassifier;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.time.Instant;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ConversationActionsTest {
    private static final String ID = "ID";
    private static final String TEXT = "TEXT";
    private static final Person PERSON = new Person.Builder().setKey(TEXT).build();
    private static final ZonedDateTime TIME =
            ZonedDateTime.ofInstant(Instant.now(), ZoneId.systemDefault());
    private static final float FLOAT_TOLERANCE = 0.01f;

    private static final Bundle EXTRAS = new Bundle();
    private static final PendingIntent PENDING_INTENT = PendingIntent.getActivity(
            InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);

    private static final RemoteAction REMOTE_ACTION = new RemoteAction(
            Icon.createWithData(new byte[0], 0, 0),
            TEXT,
            TEXT,
            PENDING_INTENT);

    static {
        EXTRAS.putString(TEXT, TEXT);
    }

    @Test
    public void testMessage_full() {
        ConversationActions.Message message =
                new ConversationActions.Message.Builder(PERSON)
                        .setText(TEXT)
                        .setExtras(EXTRAS)
                        .setReferenceTime(TIME)
                        .build();

        ConversationActions.Message recovered = parcelizeDeparcelize(message,
                ConversationActions.Message.CREATOR);

        assertFullMessage(message);
        assertFullMessage(recovered);
    }

    @Test
    public void testMessage_minimal() {
        ConversationActions.Message message =
                new ConversationActions.Message.Builder(PERSON).build();

        ConversationActions.Message recovered = parcelizeDeparcelize(message,
                ConversationActions.Message.CREATOR);

        assertMinimalMessage(message);
        assertMinimalMessage(recovered);
    }

    @Test
    public void testTypeConfig_full() {
        TextClassifier.EntityConfig typeConfig =
                new TextClassifier.EntityConfig.Builder()
                        .setIncludedTypes(
                                Collections.singletonList(ConversationAction.TYPE_OPEN_URL))
                        .setExcludedTypes(
                                Collections.singletonList(ConversationAction.TYPE_CALL_PHONE))
                        .build();

        TextClassifier.EntityConfig recovered =
                parcelizeDeparcelize(typeConfig, TextClassifier.EntityConfig.CREATOR);

        assertFullTypeConfig(typeConfig);
        assertFullTypeConfig(recovered);
    }

    @Test
    public void testTypeConfig_full_notIncludeTypesFromTextClassifier() {
        TextClassifier.EntityConfig typeConfig =
                new TextClassifier.EntityConfig.Builder()
                        .includeTypesFromTextClassifier(false)
                        .setIncludedTypes(
                                Collections.singletonList(ConversationAction.TYPE_OPEN_URL))
                        .setExcludedTypes(
                                Collections.singletonList(ConversationAction.TYPE_CALL_PHONE))
                        .build();

        TextClassifier.EntityConfig recovered =
                parcelizeDeparcelize(typeConfig, TextClassifier.EntityConfig.CREATOR);

        assertFullTypeConfig_notIncludeTypesFromTextClassifier(typeConfig);
        assertFullTypeConfig_notIncludeTypesFromTextClassifier(recovered);
    }

    @Test
    public void testTypeConfig_minimal() {
        TextClassifier.EntityConfig typeConfig =
                new TextClassifier.EntityConfig.Builder().build();

        TextClassifier.EntityConfig recovered =
                parcelizeDeparcelize(typeConfig, TextClassifier.EntityConfig.CREATOR);

        assertMinimalTypeConfig(typeConfig);
        assertMinimalTypeConfig(recovered);
    }

    @Test
    public void testRequest_minimal() {
        ConversationActions.Message message =
                new ConversationActions.Message.Builder(PERSON)
                        .setText(TEXT)
                        .build();
        ConversationActions.Request request =
                new ConversationActions.Request.Builder(Collections.singletonList(message))
                        .setMaxSuggestions(-1) // The default value.
                        .build();

        ConversationActions.Request recovered =
                parcelizeDeparcelize(request, ConversationActions.Request.CREATOR);

        assertMinimalRequest(request);
        assertMinimalRequest(recovered);
    }

    @Test
    public void testRequest_full() {
        ConversationActions.Message message =
                new ConversationActions.Message.Builder(PERSON)
                        .setText(TEXT)
                        .build();
        TextClassifier.EntityConfig typeConfig =
                new TextClassifier.EntityConfig.Builder()
                        .includeTypesFromTextClassifier(false)
                        .build();
        ConversationActions.Request request =
                new ConversationActions.Request.Builder(Collections.singletonList(message))
                        .setHints(
                                Collections.singletonList(
                                        ConversationActions.Request.HINT_FOR_IN_APP))
                        .setMaxSuggestions(10)
                        .setTypeConfig(typeConfig)
                        .setExtras(EXTRAS)
                        .build();

        ConversationActions.Request recovered =
                parcelizeDeparcelize(request, ConversationActions.Request.CREATOR);

        assertFullRequest(request);
        assertFullRequest(recovered);
    }

    @Test
    public void testConversationAction_minimal() {
        ConversationAction conversationAction =
                new ConversationAction.Builder(
                        ConversationAction.TYPE_CALL_PHONE)
                        .build();

        ConversationAction recovered =
                parcelizeDeparcelize(conversationAction,
                        ConversationAction.CREATOR);

        assertMinimalConversationAction(conversationAction);
        assertMinimalConversationAction(recovered);
    }

    @Test
    public void testConversationAction_full() {
        ConversationAction conversationAction =
                new ConversationAction.Builder(
                        ConversationAction.TYPE_CALL_PHONE)
                        .setConfidenceScore(1.0f)
                        .setTextReply(TEXT)
                        .setAction(REMOTE_ACTION)
                        .setExtras(EXTRAS)
                        .build();

        ConversationAction recovered =
                parcelizeDeparcelize(conversationAction,
                        ConversationAction.CREATOR);

        assertFullConversationAction(conversationAction);
        assertFullConversationAction(recovered);
    }

    @Test
    public void testConversationActions_full() {
        ConversationAction conversationAction =
                new ConversationAction.Builder(
                        ConversationAction.TYPE_CALL_PHONE)
                        .build();

        ConversationActions conversationActions =
                new ConversationActions(Arrays.asList(conversationAction), ID);

        ConversationActions recovered =
                parcelizeDeparcelize(conversationActions, ConversationActions.CREATOR);

        assertFullConversationActions(conversationActions);
        assertFullConversationActions(recovered);
    }

    @Test
    public void testConversationActions_minimal() {
        ConversationAction conversationAction =
                new ConversationAction.Builder(
                        ConversationAction.TYPE_CALL_PHONE)
                        .build();

        ConversationActions conversationActions =
                new ConversationActions(Arrays.asList(conversationAction), null);

        ConversationActions recovered =
                parcelizeDeparcelize(conversationActions, ConversationActions.CREATOR);

        assertMinimalConversationActions(conversationActions);
        assertMinimalConversationActions(recovered);
    }

    private void assertFullMessage(ConversationActions.Message message) {
        assertThat(message.getText().toString()).isEqualTo(TEXT);
        assertThat(message.getAuthor()).isEqualTo(PERSON);
        assertThat(message.getExtras().keySet()).containsExactly(TEXT);
        assertThat(message.getReferenceTime()).isEqualTo(TIME);
    }

    private void assertMinimalMessage(ConversationActions.Message message) {
        assertThat(message.getAuthor()).isEqualTo(PERSON);
        assertThat(message.getExtras().isEmpty()).isTrue();
        assertThat(message.getReferenceTime()).isNull();
    }

    private void assertFullTypeConfig(TextClassifier.EntityConfig typeConfig) {
        List<String> extraTypesFromTextClassifier = Arrays.asList(
                ConversationAction.TYPE_CALL_PHONE,
                ConversationAction.TYPE_CREATE_REMINDER);

        Collection<String> resolvedTypes =
                typeConfig.resolveEntityListModifications(extraTypesFromTextClassifier);

        assertThat(typeConfig.shouldIncludeTypesFromTextClassifier()).isTrue();
        assertThat(typeConfig.resolveEntityListModifications(Collections.emptyList()))
                .containsExactly(ConversationAction.TYPE_OPEN_URL);
        assertThat(resolvedTypes).containsExactly(
                ConversationAction.TYPE_OPEN_URL, ConversationAction.TYPE_CREATE_REMINDER);
    }

    private void assertFullTypeConfig_notIncludeTypesFromTextClassifier(
            TextClassifier.EntityConfig typeConfig) {
        List<String> extraTypesFromTextClassifier = Arrays.asList(
                ConversationAction.TYPE_CALL_PHONE,
                ConversationAction.TYPE_CREATE_REMINDER);

        Collection<String> resolvedTypes =
                typeConfig.resolveEntityListModifications(extraTypesFromTextClassifier);

        assertThat(typeConfig.shouldIncludeTypesFromTextClassifier()).isFalse();
        assertThat(typeConfig.resolveEntityListModifications(Collections.emptyList()))
                .containsExactly(ConversationAction.TYPE_OPEN_URL);
        assertThat(resolvedTypes).containsExactly(ConversationAction.TYPE_OPEN_URL);
    }

    private void assertMinimalTypeConfig(TextClassifier.EntityConfig typeConfig) {
        assertThat(typeConfig.shouldIncludeTypesFromTextClassifier()).isTrue();
        assertThat(typeConfig.resolveEntityListModifications(Collections.emptyList())).isEmpty();
        assertThat(typeConfig.resolveEntityListModifications(
                Collections.singletonList(ConversationAction.TYPE_OPEN_URL))).containsExactly(
                ConversationAction.TYPE_OPEN_URL);
    }

    private void assertMinimalRequest(ConversationActions.Request request) {
        assertThat(request.getConversation()).hasSize(1);
        assertThat(request.getConversation().get(0).getText().toString()).isEqualTo(TEXT);
        assertThat(request.getConversation().get(0).getAuthor()).isEqualTo(PERSON);
        assertThat(request.getHints()).isEmpty();
        assertThat(request.getMaxSuggestions()).isEqualTo(-1);
        assertThat(request.getTypeConfig()).isNotNull();
        assertThat(request.getExtras().size()).isEqualTo(0);
    }

    private void assertFullRequest(ConversationActions.Request request) {
        assertThat(request.getConversation()).hasSize(1);
        assertThat(request.getConversation().get(0).getText().toString()).isEqualTo(TEXT);
        assertThat(request.getConversation().get(0).getAuthor()).isEqualTo(PERSON);
        assertThat(request.getHints()).containsExactly(ConversationActions.Request.HINT_FOR_IN_APP);
        assertThat(request.getMaxSuggestions()).isEqualTo(10);
        assertThat(request.getTypeConfig().shouldIncludeTypesFromTextClassifier()).isFalse();
        assertThat(request.getExtras().keySet()).containsExactly(TEXT);
    }

    private void assertMinimalConversationAction(
            ConversationAction conversationAction) {
        assertThat(conversationAction.getAction()).isNull();
        assertThat(conversationAction.getConfidenceScore()).isWithin(FLOAT_TOLERANCE).of(0.0f);
        assertThat(conversationAction.getType()).isEqualTo(ConversationAction.TYPE_CALL_PHONE);
    }

    private void assertFullConversationAction(
            ConversationAction conversationAction) {
        assertThat(conversationAction.getAction().getTitle()).isEqualTo(TEXT);
        assertThat(conversationAction.getConfidenceScore()).isWithin(FLOAT_TOLERANCE).of(1.0f);
        assertThat(conversationAction.getType()).isEqualTo(ConversationAction.TYPE_CALL_PHONE);
        assertThat(conversationAction.getTextReply()).isEqualTo(TEXT);
        assertThat(conversationAction.getExtras().keySet()).containsExactly(TEXT);
    }

    private void assertMinimalConversationActions(ConversationActions conversationActions) {
        assertThat(conversationActions.getConversationActions()).hasSize(1);
        assertThat(conversationActions.getConversationActions().get(0).getType())
                .isEqualTo(ConversationAction.TYPE_CALL_PHONE);
        assertThat(conversationActions.getId()).isNull();
    }

    private void assertFullConversationActions(ConversationActions conversationActions) {
        assertThat(conversationActions.getConversationActions()).hasSize(1);
        assertThat(conversationActions.getConversationActions().get(0).getType())
                .isEqualTo(ConversationAction.TYPE_CALL_PHONE);
        assertThat(conversationActions.getId()).isEqualTo(ID);
    }

    private <T extends Parcelable> T parcelizeDeparcelize(
            T parcelable, Parcelable.Creator<T> creator) {
        Parcel parcel = Parcel.obtain();
        parcelable.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        return creator.createFromParcel(parcel);
    }
}
