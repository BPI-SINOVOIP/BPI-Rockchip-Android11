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

import static android.view.textclassifier.ConversationActions.Message.PERSON_USER_SELF;
import static com.google.common.truth.Truth.assertThat;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Person;
import android.app.RemoteAction;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.service.notification.NotificationAssistantService;
import android.service.notification.StatusBarNotification;
import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.ConversationActions;
import android.view.textclassifier.ConversationActions.Message;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import com.google.common.collect.ImmutableList;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class SmartSuggestionsHelperTest {
  private static final String PACKAGE_NAME = "random.app";
  private static final String MESSAGE = "Where are you?";
  private static final CharSequence SMART_REPLY = "Home";
  private static final CharSequence ACTION_TITLE = "Open";

  private static final String RESULT_ID = "id";
  private static final float REPLY_SCORE = 0.7f;
  private static final float ACTION_SCORE = 1.0f;
  private final Context context = ApplicationProvider.getApplicationContext();
  private final FakeTextClassifier fakeTextClassifier = new FakeTextClassifier();
  private final TestConfig config = new TestConfig();
  private TestableSmartSuggestionsHelper smartActions;
  private Notification.Builder notificationBuilder;

  @Before
  public void setup() {
    TextClassificationManager textClassificationManager =
        context.getSystemService(TextClassificationManager.class);
    textClassificationManager.setTextClassifier(fakeTextClassifier);
    smartActions = new TestableSmartSuggestionsHelper(context, config);
    notificationBuilder = new Notification.Builder(context, "id");
  }

  static class TestableSmartSuggestionsHelper extends SmartSuggestionsHelper {
    private int numOfSessionsCreated = 0;

    TestableSmartSuggestionsHelper(Context context, SmartSuggestionsConfig config) {
      super(context, config);
    }

    @Override
    TextClassifier createTextClassificationSession() {
      numOfSessionsCreated += 1;
      return super.createTextClassificationSession();
    }

    int getNumOfSessionsCreated() {
      return numOfSessionsCreated;
    }
  }

  @Test
  public void onNotificationEnqueued_notMessageCategory() {
    Notification notification = notificationBuilder.setContentText(MESSAGE).build();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertThat(smartSuggestions.getReplies()).isEmpty();
    assertThat(smartSuggestions.getActions()).isEmpty();
    // Ideally, we should verify that createTextClassificationSession
    assertThat(smartActions.getNumOfSessionsCreated()).isEqualTo(0);
  }

  @Test
  public void onNotificationEnqueued_fromSystem() {
    Notification notification =
        notificationBuilder
            .setContentText(MESSAGE)
            .setCategory(Notification.CATEGORY_MESSAGE)
            .setActions(createReplyAction())
            .build();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, "android");

    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertThat(smartSuggestions.getReplies()).isEmpty();
    assertThat(smartSuggestions.getActions()).isEmpty();
    assertThat(smartActions.getNumOfSessionsCreated()).isEqualTo(0);
  }

  @Test
  public void onNotificationEnqueued_noInlineReply() {
    Notification notification =
        notificationBuilder
            .setContentText(MESSAGE)
            .setCategory(Notification.CATEGORY_MESSAGE)
            .build();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertThat(smartSuggestions.getReplies()).isEmpty();
    assertAdjustmentWithSmartAction(smartSuggestions);
    assertThat(smartActions.getNumOfSessionsCreated()).isEqualTo(1);
  }

  @Test
  public void onNotificationEnqueued_messageCategoryNotification() {
    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertAdjustmentWithSmartReply(smartSuggestions);
    assertAdjustmentWithSmartAction(smartSuggestions);
    ConversationActions.Request request = fakeTextClassifier.getLastRequest();
    List<Message> messages = request.getConversation();
    assertThat(messages).hasSize(1);
    assertThat(messages.get(0).getText().toString()).isEqualTo(MESSAGE);
    assertThat(smartActions.getNumOfSessionsCreated()).isEqualTo(1);
  }

  @Test
  public void onNotificationEnqueued_messageStyleNotification() {
    Person me = new Person.Builder().setName("Me").build();
    Person userA = new Person.Builder().setName("A").build();
    Person userB = new Person.Builder().setName("B").build();
    Notification.MessagingStyle style =
        new Notification.MessagingStyle(me)
            .addMessage("firstMessage", 1000, (Person) null)
            .addMessage("secondMessage", 2000, me)
            .addMessage("thirdMessage", 3000, userA)
            .addMessage("fourthMessage", 4000, userB);
    Notification notification =
        notificationBuilder
            .setContentText("You have three new messages")
            .setStyle(style)
            .setActions(createReplyAction())
            .build();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);
    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertAdjustmentWithSmartReply(smartSuggestions);
    assertAdjustmentWithSmartAction(smartSuggestions);
    ConversationActions.Request request = fakeTextClassifier.getLastRequest();
    List<ConversationActions.Message> messages = request.getConversation();
    assertThat(messages).hasSize(4);

    assertMessage(messages.get(0), "firstMessage", PERSON_USER_SELF, 1000);
    assertMessage(messages.get(1), "secondMessage", PERSON_USER_SELF, 2000);
    assertMessage(messages.get(2), "thirdMessage", userA, 3000);
    assertMessage(messages.get(3), "fourthMessage", userB, 4000);
    assertThat(smartActions.getNumOfSessionsCreated()).isEqualTo(1);
  }

  @Test
  public void onNotificationEnqueued_lastMessageFromLocalUser() {
    Person me = new Person.Builder().setName("Me").build();
    Person userA = new Person.Builder().setName("A").build();
    Notification.MessagingStyle style =
        new Notification.MessagingStyle(me)
            .addMessage("firstMessage", 1000, userA)
            .addMessage("secondMessage", 2000, me);
    Notification notification =
        notificationBuilder
            .setContentText("You have two new messages")
            .setStyle(style)
            .setActions(createReplyAction())
            .build();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertThat(smartSuggestions.getReplies()).isEmpty();
    assertThat(smartSuggestions.getActions()).isEmpty();
    assertThat(smartActions.getNumOfSessionsCreated()).isEqualTo(0);
  }

  @Test
  public void onNotificationEnqueued_messageStyleNotification_missingPerson() {
    Person me = new Person.Builder().setName("Me").build();
    Notification.MessagingStyle style =
        new Notification.MessagingStyle(me).addMessage("message", 1000, (Person) null);
    Notification notification =
        notificationBuilder
            .setContentText("You have one new message")
            .setStyle(style)
            .setActions(createReplyAction())
            .build();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertThat(smartSuggestions.getReplies()).isEmpty();
    assertThat(smartSuggestions.getActions()).isEmpty();
    assertThat(smartActions.getNumOfSessionsCreated()).isEqualTo(0);
  }

  @Test
  public void onSuggestedReplySent() {
    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    smartActions.onNotificationEnqueued(statusBarNotification);
    smartActions.onSuggestedReplySent(
        statusBarNotification.getKey(),
        SMART_REPLY,
        NotificationAssistantService.SOURCE_FROM_ASSISTANT);

    List<TextClassifierEvent> textClassifierEvents = fakeTextClassifier.getTextClassifierEvents();
    assertThat(textClassifierEvents).hasSize(2);
    TextClassifierEvent firstEvent = textClassifierEvents.get(0);
    assertThat(firstEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_GENERATED);
    assertThat(firstEvent.getEntityTypes())
        .asList()
        .containsExactly(ConversationAction.TYPE_TEXT_REPLY, ConversationAction.TYPE_OPEN_URL);
    TextClassifierEvent secondEvent = textClassifierEvents.get(1);
    assertThat(secondEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_SMART_ACTION);
    assertThat(secondEvent.getEntityTypes()[0]).isEqualTo(ConversationAction.TYPE_TEXT_REPLY);
  }

  @Test
  public void onSuggestedReplySent_noMatchingSession() {
    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    smartActions.onNotificationEnqueued(statusBarNotification);
    smartActions.onSuggestedReplySent(
        "something_else", MESSAGE, NotificationAssistantService.SOURCE_FROM_ASSISTANT);

    // No matching session, so TYPE_SMART_ACTION should not be logged.
    List<TextClassifierEvent> textClassifierEvents = fakeTextClassifier.getTextClassifierEvents();
    assertThat(textClassifierEvents).hasSize(1);
    TextClassifierEvent firstEvent = textClassifierEvents.get(0);
    assertThat(firstEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_GENERATED);
  }

  @Test
  public void onNotificationDirectReply() {
    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    smartActions.onNotificationEnqueued(statusBarNotification);
    smartActions.onNotificationDirectReplied(statusBarNotification.getKey());

    List<TextClassifierEvent> textClassifierEvents = fakeTextClassifier.getTextClassifierEvents();
    assertThat(textClassifierEvents).hasSize(2);
    TextClassifierEvent firstEvent = textClassifierEvents.get(0);
    assertThat(firstEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_GENERATED);
    TextClassifierEvent secondEvent = textClassifierEvents.get(1);
    assertThat(secondEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_MANUAL_REPLY);
  }

  @Test
  public void oNotificationExpansionChanged_expanded() {
    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    smartActions.onNotificationEnqueued(statusBarNotification);
    smartActions.onNotificationExpansionChanged(statusBarNotification, /* isExpanded= */ true);

    List<TextClassifierEvent> textClassifierEvents = fakeTextClassifier.getTextClassifierEvents();
    assertThat(textClassifierEvents).hasSize(2);
    TextClassifierEvent firstEvent = textClassifierEvents.get(0);
    assertThat(firstEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_GENERATED);
    TextClassifierEvent secondEvent = textClassifierEvents.get(1);
    assertThat(secondEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
  }

  @Test
  public void oNotificationExpansionChanged_notExpanded() {
    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    smartActions.onNotificationEnqueued(statusBarNotification);
    smartActions.onNotificationExpansionChanged(statusBarNotification, /* isExpanded= */ false);

    List<TextClassifierEvent> textClassifierEvents = fakeTextClassifier.getTextClassifierEvents();
    assertThat(textClassifierEvents).hasSize(1);
    TextClassifierEvent firstEvent = textClassifierEvents.get(0);
    assertThat(firstEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_GENERATED);
  }

  @Test
  public void oNotificationExpansionChanged_expanded_logShownEventOnce() {
    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);

    smartActions.onNotificationEnqueued(statusBarNotification);
    smartActions.onNotificationExpansionChanged(statusBarNotification, /* isExpanded= */ true);
    smartActions.onNotificationExpansionChanged(statusBarNotification, /* isExpanded= */ true);

    List<TextClassifierEvent> textClassifierEvents = fakeTextClassifier.getTextClassifierEvents();
    assertThat(textClassifierEvents).hasSize(2);
    TextClassifierEvent firstEvent = textClassifierEvents.get(0);
    assertThat(firstEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_GENERATED);
    TextClassifierEvent secondEvent = textClassifierEvents.get(1);
    assertThat(secondEvent.getEventType()).isEqualTo(TextClassifierEvent.TYPE_ACTIONS_SHOWN);
  }

  @Test
  public void copyCodeAction() {
    Bundle extras = new Bundle();
    Bundle entitiesExtras = new Bundle();
    entitiesExtras.putString(SmartSuggestionsHelper.KEY_TEXT, "12345");
    extras.putParcelable(SmartSuggestionsHelper.ENTITIES_EXTRAS, entitiesExtras);
    ConversationAction conversationAction =
        new ConversationAction.Builder(SmartSuggestionsHelper.TYPE_COPY).setExtras(extras).build();
    fakeTextClassifier.setSuggestConversationActionsResponse(
        new ConversationActions(ImmutableList.of(conversationAction), RESULT_ID));

    Notification notification = createMessageCategoryNotification();
    StatusBarNotification statusBarNotification =
        createStatusBarNotification(notification, PACKAGE_NAME);
    SmartSuggestions smartSuggestions = smartActions.onNotificationEnqueued(statusBarNotification);

    assertThat(smartSuggestions.getActions()).hasSize(1);
    assertThat(smartSuggestions.getActions().get(0).title.toString()).isEqualTo("12345");
  }

  @Ignore // Disabled because it is way too slow to run on an emulator.
  @Test
  public void noBinderLeakage() {
    // Use the real text classifier from system.
    TextClassificationManager textClassificationManager =
        context.getSystemService(TextClassificationManager.class);
    textClassificationManager.setTextClassifier(null);

    // System server crashes when there are more than 20,000 leaked binder proxy.
    // See
    // http://cs/android/frameworks/base/core/java/android/os/BinderProxy.java?l=73&rcl=ae52315c8c7d0391bd3c7bca0525a98eeb4cd840.
    for (int i = 0; i < 20000; i++) {
      Notification notification = createMessageCategoryNotification();
      StatusBarNotification statusBarNotification =
          createStatusBarNotification(notification, PACKAGE_NAME);
      smartActions.onNotificationEnqueued(statusBarNotification);
    }
  }

  private Notification createMessageCategoryNotification() {
    return notificationBuilder
        .setContentText(MESSAGE)
        .setCategory(Notification.CATEGORY_MESSAGE)
        .setActions(createReplyAction())
        .build();
  }

  private static StatusBarNotification createStatusBarNotification(
      Notification notification, String packageName) {
    return new StatusBarNotification(
        packageName,
        packageName,
        /* id= */ 2,
        "tag",
        /* uid= */ 1,
        /* initialPid= */ 1,
        /* score= */ 1,
        notification,
        Process.myUserHandle(),
        System.currentTimeMillis());
  }

  private Notification.Action createReplyAction() {
    return new Notification.Action.Builder(
            Icon.createWithResource(context, android.R.drawable.stat_sys_warning),
            "Reply",
            PendingIntent.getActivity(context, 0, new Intent(context, this.getClass()), 0))
        .addRemoteInput(new RemoteInput.Builder("result").setAllowFreeFormInput(true).build())
        .build();
  }

  private static void assertMessage(
      ConversationActions.Message subject,
      String expectedMessage,
      Person expectedAuthor,
      long expectedReferenceTime) {
    assertThat(subject.getText().toString()).isEqualTo(expectedMessage);
    assertThat(subject.getAuthor()).isEqualTo(expectedAuthor);
    assertThat(subject.getReferenceTime().toInstant().toEpochMilli())
        .isEqualTo(expectedReferenceTime);
  }

  private static void assertAdjustmentWithSmartReply(SmartSuggestions smartSuggestions) {
    assertThat(smartSuggestions.getReplies()).containsExactly(SMART_REPLY);
  }

  private static void assertAdjustmentWithSmartAction(SmartSuggestions smartSuggestions) {
    assertThat(smartSuggestions.getActions().get(0).title.toString())
        .isEqualTo(ACTION_TITLE.toString());
  }

  private static class FakeTextClassifier implements TextClassifier {

    private ConversationActions.Request lastRequest;
    private final List<TextClassifierEvent> textClassifierEvents = new ArrayList<>();
    private ConversationActions conversationActions;

    @Override
    public ConversationActions suggestConversationActions(ConversationActions.Request request) {
      lastRequest = request;
      if (conversationActions != null) {
        return conversationActions;
      }
      Collection<String> types =
          request
              .getTypeConfig()
              .resolveEntityListModifications(
                  Arrays.asList(
                      ConversationAction.TYPE_OPEN_URL, ConversationAction.TYPE_TEXT_REPLY));
      List<ConversationAction> result = new ArrayList<>();

      if (types.contains(ConversationAction.TYPE_TEXT_REPLY)) {
        ConversationAction smartReply =
            new ConversationAction.Builder(ConversationAction.TYPE_TEXT_REPLY)
                .setTextReply(SMART_REPLY)
                .setConfidenceScore(REPLY_SCORE)
                .build();
        result.add(smartReply);
      }
      if (types.contains(ConversationAction.TYPE_OPEN_URL)) {
        Intent webIntent = new Intent(Intent.ACTION_VIEW).setData(Uri.parse("www.android.com"));
        ConversationAction smartAction =
            new ConversationAction.Builder(ConversationAction.TYPE_OPEN_URL)
                .setConfidenceScore(ACTION_SCORE)
                .setAction(
                    new RemoteAction(
                        Icon.createWithData(new byte[0], 0, 0),
                        ACTION_TITLE,
                        ACTION_TITLE,
                        PendingIntent.getActivity(
                            ApplicationProvider.getApplicationContext(), 0, webIntent, 0)))
                .build();
        result.add(smartAction);
      }
      return new ConversationActions(result, RESULT_ID);
    }

    @Override
    public void onTextClassifierEvent(TextClassifierEvent event) {
      textClassifierEvents.add(event);
    }

    private void setSuggestConversationActionsResponse(ConversationActions conversationActions) {
      this.conversationActions = conversationActions;
    }

    private ConversationActions.Request getLastRequest() {
      return lastRequest;
    }

    private List<TextClassifierEvent> getTextClassifierEvents() {
      return ImmutableList.copyOf(textClassifierEvents);
    }
  }

  private static class TestConfig implements SmartSuggestionsConfig {
    private final boolean shouldGenerateReplies = true;
    private final boolean shouldGenerateActions = true;
    private final int maxSuggestions = 3;
    private final int maxMessagesToExtract = 5;

    @Override
    public boolean shouldGenerateReplies() {
      return shouldGenerateReplies;
    }

    @Override
    public boolean shouldGenerateActions() {
      return shouldGenerateActions;
    }

    @Override
    public int getMaxSuggestions() {
      return maxSuggestions;
    }

    @Override
    public int getMaxMessagesToExtract() {
      return maxMessagesToExtract;
    }
  }
}
