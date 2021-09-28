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

import android.app.Notification;
import android.app.Notification.MessagingStyle.Message;
import android.app.PendingIntent;
import android.app.Person;
import android.app.RemoteAction;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.Process;
import android.service.notification.StatusBarNotification;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.Log;
import android.util.LruCache;
import android.util.Pair;
import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.ConversationActions;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.Iterables;
import java.time.Instant;
import java.time.ZoneOffset;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import javax.annotation.Nullable;

/**
 * Generates suggestions from incoming notifications and handles related logging.
 *
 * <p>This class is not thread-safe. Either call methods in this class in a single worker thread or
 * guard all the calls with the same lock.
 */
public class SmartSuggestionsHelper {
  private static final String TAG = "SmartSuggestionsHelper";

  static final String ENTITIES_EXTRAS = "entities-extras";
  static final String KEY_ACTION_TYPE = "action_type";
  static final String KEY_ACTION_SCORE = "action_score";
  static final String KEY_TEXT = "text";
  static final String NOTIFICATION_KEY = "notificationKey";
  // Copied from ConversationAction.java
  static final String TYPE_COPY = "copy";

  // If a notification has any of these flags set, it's inelgibile for actions being added.
  private static final int FLAG_MASK_INELGIBILE_FOR_ACTIONS =
      Notification.FLAG_ONGOING_EVENT
          | Notification.FLAG_FOREGROUND_SERVICE
          | Notification.FLAG_GROUP_SUMMARY
          | Notification.FLAG_NO_CLEAR;
  private static final int MAX_RESULT_ID_TO_CACHE = 20;
  private static final ImmutableList<String> HINTS =
      ImmutableList.of(ConversationActions.Request.HINT_FOR_NOTIFICATION);
  private static final SuggestConversationActionsResult EMPTY_SUGGEST_CONVERSATION_ACTION_RESULT =
      new SuggestConversationActionsResult(
          Optional.empty(), new ConversationActions(ImmutableList.of(), /* id= */ null));

  private final Context context;
  private final TextClassificationManager textClassificationManager;
  private final SmartSuggestionsConfig config;
  private final LruCache<String, SmartSuggestionsLogSession> sessionCache =
      new LruCache<String, SmartSuggestionsLogSession>(MAX_RESULT_ID_TO_CACHE) {
        @Override
        protected void entryRemoved(
            boolean evicted,
            String key,
            SmartSuggestionsLogSession oldSession,
            SmartSuggestionsLogSession newSession) {
          oldSession.destroy();
        }
      };
  private final TextClassificationContext textClassificationContext;

  public SmartSuggestionsHelper(Context context, SmartSuggestionsConfig config) {
    this.context = context;
    textClassificationManager = this.context.getSystemService(TextClassificationManager.class);
    this.config = config;
    this.textClassificationContext =
        new TextClassificationContext.Builder(
                context.getPackageName(), TextClassifier.WIDGET_TYPE_NOTIFICATION)
            .build();
  }

  /**
   * Notifies a notification is enqueued and returns some suggestions based on the conversation in
   * the given status bar notification.
   */
  public SmartSuggestions onNotificationEnqueued(StatusBarNotification statusBarNotification) {
    // Whenever onNotificationEnqueued() is called again on the same notification key, its
    // previous session is ended.
    sessionCache.remove(statusBarNotification.getKey());

    boolean eligibleForReplyAdjustment =
        config.shouldGenerateReplies() && isEligibleForReplyAdjustment(statusBarNotification);
    boolean eligibleForActionAdjustment =
        config.shouldGenerateActions() && isEligibleForActionAdjustment(statusBarNotification);

    SuggestConversationActionsResult suggestConversationActionsResult =
        suggestConversationActions(
            statusBarNotification, eligibleForReplyAdjustment, eligibleForActionAdjustment);

    String resultId = suggestConversationActionsResult.conversationActions.getId();
    List<ConversationAction> conversationActions =
        suggestConversationActionsResult.conversationActions.getConversationActions();

    ArrayList<CharSequence> replies = new ArrayList<>();
    Map<CharSequence, Float> repliesScore = new ArrayMap<>();
    for (ConversationAction conversationAction : conversationActions) {
      CharSequence textReply = conversationAction.getTextReply();
      if (TextUtils.isEmpty(textReply)) {
        continue;
      }
      replies.add(textReply);
      repliesScore.put(textReply, conversationAction.getConfidenceScore());
    }

    ArrayList<Notification.Action> actions = new ArrayList<>();
    for (ConversationAction conversationAction : conversationActions) {
      if (!TextUtils.isEmpty(conversationAction.getTextReply())) {
        continue;
      }
      Notification.Action notificationAction;
      RemoteAction remoteAction = conversationAction.getAction();
      if (remoteAction == null) {
        notificationAction = createNotificationActionWithoutRemoteAction(conversationAction);
      } else {
        notificationAction =
            createNotificationActionFromRemoteAction(
                remoteAction,
                conversationAction.getType(),
                conversationAction.getConfidenceScore());
      }
      if (notificationAction != null) {
        actions.add(notificationAction);
      }
    }

    suggestConversationActionsResult.textClassifier.ifPresent(
        textClassifier -> {
          if (TextUtils.isEmpty(resultId)) {
            // Missing the result id, skip logging.
            textClassifier.destroy();
          } else {
            SmartSuggestionsLogSession session =
                new SmartSuggestionsLogSession(
                    resultId,
                    repliesScore,
                    textClassifier,
                    textClassificationContext);
            session.onSuggestionsGenerated(conversationActions);

            // Store the session if we expect more logging from it, destroy it otherwise.
            if (!conversationActions.isEmpty()
                && suggestionsMightBeUsedInNotification(
                    statusBarNotification, !actions.isEmpty(), !replies.isEmpty())) {
              sessionCache.put(statusBarNotification.getKey(), session);
            } else {
              session.destroy();
            }
          }
        });

    return new SmartSuggestions(replies, actions);
  }

  /**
   * Creates notification action from ConversationAction that does not come up a RemoteAction. It
   * could happen because we don't have common intents for some actions, like copying text.
   */
  @Nullable
  private Notification.Action createNotificationActionWithoutRemoteAction(
      ConversationAction conversationAction) {
    if (TYPE_COPY.equals(conversationAction.getType())) {
      return createCopyCodeAction(conversationAction);
    }
    return null;
  }

  @Nullable
  private Notification.Action createCopyCodeAction(ConversationAction conversationAction) {
    Bundle extras = conversationAction.getExtras();
    Bundle entitiesExtas = extras.getParcelable(ENTITIES_EXTRAS);
    if (entitiesExtas == null) {
      return null;
    }
    String code = entitiesExtas.getString(KEY_TEXT);
    if (TextUtils.isEmpty(code)) {
      return null;
    }
    String contentDescription = context.getString(R.string.tc_notif_copy_code_desc, code);
    Intent intent = new Intent(context, CopyCodeActivity.class);
    intent.putExtra(Intent.EXTRA_TEXT, code);

    RemoteAction remoteAction =
        new RemoteAction(
            Icon.createWithResource(context, R.drawable.tc_notif_ic_menu_copy_material),
            code,
            contentDescription,
            PendingIntent.getActivity(
                context, code.hashCode(), intent, PendingIntent.FLAG_UPDATE_CURRENT));

    return createNotificationActionFromRemoteAction(
        remoteAction, TYPE_COPY, conversationAction.getConfidenceScore());
  }

  /**
   * Returns whether the suggestion might be used in the notifications in SysUI.
   *
   * <p>Currently, NAS has no idea if suggestions will actually be used in the notification, and
   * thus this function tries to make a heuristic. This function tries to optimize the precision,
   * that means when it is unsure, it will return false. The objective is to avoid false positive,
   * which could pollute the log and CTR as we are logging click rate of suggestions that could be
   * never visible to users. On the other hand, it is fine to have false negative because it would
   * be just like sampling.
   */
  private static boolean suggestionsMightBeUsedInNotification(
      StatusBarNotification statusBarNotification, boolean hasSmartAction, boolean hasSmartReply) {
    Notification notification = statusBarNotification.getNotification();
    boolean hasAppGeneratedContextualActions = !notification.getContextualActions().isEmpty();

    Pair<RemoteInput, Notification.Action> freeformRemoteInputAndAction =
        notification.findRemoteInputActionPair(/* requiresFreeform */ true);
    boolean hasAppGeneratedReplies = false;
    boolean allowGeneratedReplies = false;
    if (freeformRemoteInputAndAction != null) {
      RemoteInput freeformRemoteInput = freeformRemoteInputAndAction.first;
      Notification.Action actionWithFreeformRemoteInput = freeformRemoteInputAndAction.second;
      CharSequence[] choices = freeformRemoteInput.getChoices();
      hasAppGeneratedReplies = (choices != null && choices.length > 0);
      allowGeneratedReplies = actionWithFreeformRemoteInput.getAllowGeneratedReplies();
    }

    if (hasAppGeneratedReplies || hasAppGeneratedContextualActions) {
      return false;
    }
    return (hasSmartAction && notification.getAllowSystemGeneratedContextualActions())
        || (hasSmartReply && allowGeneratedReplies);
  }

  /** Adds action adjustments based on the notification contents. */
  private SuggestConversationActionsResult suggestConversationActions(
      StatusBarNotification statusBarNotification, boolean includeReplies, boolean includeActions) {
    if (!includeReplies && !includeActions) {
      return EMPTY_SUGGEST_CONVERSATION_ACTION_RESULT;
    }
    ImmutableList<ConversationActions.Message> messages =
        extractMessages(statusBarNotification.getNotification());
    if (messages.isEmpty()) {
      return EMPTY_SUGGEST_CONVERSATION_ACTION_RESULT;
    }
    // Do not generate smart actions if the last message is from the local user.
    ConversationActions.Message lastMessage = Iterables.getLast(messages);
    if (arePersonsEqual(ConversationActions.Message.PERSON_USER_SELF, lastMessage.getAuthor())) {
      return EMPTY_SUGGEST_CONVERSATION_ACTION_RESULT;
    }

    TextClassifier.EntityConfig.Builder typeConfigBuilder =
        new TextClassifier.EntityConfig.Builder();
    if (!includeReplies) {
      typeConfigBuilder.setExcludedTypes(ImmutableList.of(ConversationAction.TYPE_TEXT_REPLY));
    } else if (!includeActions) {
      typeConfigBuilder
          .setIncludedTypes(ImmutableList.of(ConversationAction.TYPE_TEXT_REPLY))
          .includeTypesFromTextClassifier(false);
    }

    // Put the notification key into the request extras
    Bundle extra = new Bundle();
    extra.putString(NOTIFICATION_KEY, statusBarNotification.getKey());
    ConversationActions.Request request =
        new ConversationActions.Request.Builder(messages)
            .setMaxSuggestions(config.getMaxSuggestions())
            .setHints(HINTS)
            .setExtras(extra)
            .setTypeConfig(typeConfigBuilder.build())
            .build();

    TextClassifier textClassifier = createTextClassificationSession();
    return new SuggestConversationActionsResult(
        Optional.of(textClassifier), textClassifier.suggestConversationActions(request));
  }

  /**
   * Notifies that a notification has been expanded or collapsed.
   *
   * @param statusBarNotification status bar notification
   * @param isExpanded true for when a notification is expanded, false for when it is collapsed
   */
  public void onNotificationExpansionChanged(
      StatusBarNotification statusBarNotification, boolean isExpanded) {
    SmartSuggestionsLogSession session = sessionCache.get(statusBarNotification.getKey());
    if (session == null) {
      return;
    }
    session.onNotificationExpansionChanged(isExpanded);
  }

  /** Notifies that a direct reply has been sent from a notification. */
  public void onNotificationDirectReplied(String key) {
    SmartSuggestionsLogSession session = sessionCache.get(key);
    if (session == null) {
      return;
    }
    session.onDirectReplied();
  }

  /**
   * Notifies that a suggested reply has been sent.
   *
   * @param key the notification key
   * @param reply the reply that is just sent
   * @param source the source that provided the reply, e.g. SOURCE_FROM_ASSISTANT
   */
  public void onSuggestedReplySent(String key, CharSequence reply, int source) {
    SmartSuggestionsLogSession session = sessionCache.get(key);
    if (session == null) {
      return;
    }
    session.onSuggestedReplySent(reply, source);
  }

  /**
   * Notifies an action is clicked.
   *
   * @param key the notification key
   * @param action the action that is just clicked
   * @param source the source that provided the reply, e.g. SOURCE_FROM_ASSISTANT
   */
  public void onActionClicked(String key, Notification.Action action, int source) {
    SmartSuggestionsLogSession session = sessionCache.get(key);
    if (session == null) {
      return;
    }
    session.onActionClicked(action, source);
  }

  /** Clears the internal cache. */
  public void clearCache() {
    sessionCache.evictAll();
  }

  private Notification.Action createNotificationActionFromRemoteAction(
      RemoteAction remoteAction, String actionType, float score) {
    Icon icon =
        remoteAction.shouldShowIcon()
            ? remoteAction.getIcon()
            : Icon.createWithResource(context, R.drawable.tc_notif_ic_action_open);
    Bundle extras = new Bundle();
    extras.putString(KEY_ACTION_TYPE, actionType);
    extras.putFloat(KEY_ACTION_SCORE, score);
    return new Notification.Action.Builder(
            icon, remoteAction.getTitle(), remoteAction.getActionIntent())
        .setContextual(true)
        .addExtras(extras)
        .build();
  }

  /**
   * Returns whether a notification is eligible for action adjustments.
   *
   * <p>We exclude system notifications, those that get refreshed frequently, or ones that relate to
   * fundamental phone functionality where any error would result in a very negative user
   * experience.
   */
  private static boolean isEligibleForActionAdjustment(
      StatusBarNotification statusBarNotification) {
    String pkg = statusBarNotification.getPackageName();
    if (!Process.myUserHandle().equals(statusBarNotification.getUser())) {
      return false;
    }
    Notification notification = statusBarNotification.getNotification();
    if ((notification.flags & FLAG_MASK_INELGIBILE_FOR_ACTIONS) != 0) {
      return false;
    }
    if (TextUtils.isEmpty(pkg) || pkg.equals("android")) {
      return false;
    }
    // For now, we are only interested in messages.
    return NotificationUtils.isMessaging(statusBarNotification);
  }

  private static boolean isEligibleForReplyAdjustment(StatusBarNotification statusBarNotification) {
    if (!Process.myUserHandle().equals(statusBarNotification.getUser())) {
      return false;
    }
    String pkg = statusBarNotification.getPackageName();
    if (TextUtils.isEmpty(pkg) || pkg.equals("android")) {
      return false;
    }
    // For now, we are only interested in messages.
    if (!NotificationUtils.isMessaging(statusBarNotification)) {
      return false;
    }
    // Does not make sense to provide suggested replies if it is not something that can be
    // replied.
    if (!NotificationUtils.hasInlineReply(statusBarNotification)) {
      return false;
    }
    return true;
  }

  /** Returns the text most salient for action extraction in a notification. */
  private ImmutableList<ConversationActions.Message> extractMessages(Notification notification) {
    List<Message> messages =
        Message.getMessagesFromBundleArray(
            notification.extras.getParcelableArray(Notification.EXTRA_MESSAGES));
    if (messages == null || messages.isEmpty()) {
      return ImmutableList.of(
          new ConversationActions.Message.Builder(ConversationActions.Message.PERSON_USER_OTHERS)
              .setText(notification.extras.getCharSequence(Notification.EXTRA_TEXT))
              .build());
    }
    Person localUser = notification.extras.getParcelable(Notification.EXTRA_MESSAGING_PERSON);
    if (localUser == null) {
      Log.w(TAG, "EXTRA_MESSAGING_PERSON is missing, failed to extract messages.");
      return ImmutableList.of();
    }
    Deque<ConversationActions.Message> extractMessages = new ArrayDeque<>();
    for (int i = messages.size() - 1; i >= 0; i--) {
      Message message = messages.get(i);
      if (message == null) {
        continue;
      }
      Person senderPerson = message.getSenderPerson();
      // As per the javadoc of Notification.addMessage(), a null sender refers to the user
      // themselves.
      Person author =
          senderPerson == null || arePersonsEqual(localUser, senderPerson)
              ? ConversationActions.Message.PERSON_USER_SELF
              : senderPerson;
      extractMessages.push(
          new ConversationActions.Message.Builder(author)
              .setText(message.getText())
              .setReferenceTime(
                  Instant.ofEpochMilli(message.getTimestamp()).atZone(ZoneOffset.systemDefault()))
              .build());
      if (extractMessages.size() >= config.getMaxMessagesToExtract()) {
        break;
      }
    }
    return ImmutableList.copyOf(new ArrayList<>(extractMessages));
  }

  @VisibleForTesting
  TextClassifier createTextClassificationSession() {
    return textClassificationManager.createTextClassificationSession(textClassificationContext);
  }

  private static boolean arePersonsEqual(Person left, Person right) {
    return Objects.equals(left.getKey(), right.getKey())
        && TextUtils.equals(left.getName(), right.getName())
        && Objects.equals(left.getUri(), right.getUri());
  }

  /**
   * Result object of {@link #suggestConversationActions(StatusBarNotification, boolean, boolean)}.
   */
  private static class SuggestConversationActionsResult {
    /** The text classifier session that was involved to make suggestions, if any. */
    final Optional<TextClassifier> textClassifier;
    /** The resultant suggestions. */
    final ConversationActions conversationActions;

    SuggestConversationActionsResult(
        Optional<TextClassifier> textClassifier, ConversationActions conversationActions) {
      this.textClassifier = textClassifier;
      this.conversationActions = conversationActions;
    }
  }
}
