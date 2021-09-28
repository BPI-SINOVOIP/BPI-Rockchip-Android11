/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.car.messenger.common;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationCompat.Action;
import androidx.core.app.Person;
import androidx.core.graphics.drawable.IconCompat;

import com.android.car.telephony.common.TelecomUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Predicate;

/**
 * Base Interface for Message Notification Delegates.
 * Any Delegate who chooses to extend from this class is responsible for:
 * <p> device connection logic </p>
 * <p> sending and receiving messages from the connected devices </p>
 * <p> creation of {@link ConversationNotificationInfo} and {@link Message} objects </p>
 * <p> creation of {@link ConversationKey}, {@link MessageKey}, {@link SenderKey} </p>
 * <p> loading of largeIcons for each Sender per device </p>
 * <p> Mark-as-Read and Reply functionality  </p>
 **/
public class BaseNotificationDelegate {

    /** Used to reply to message. */
    public static final String ACTION_REPLY = "com.android.car.messenger.common.ACTION_REPLY";

    /** Used to clear notification state when user dismisses notification. */
    public static final String ACTION_DISMISS_NOTIFICATION =
            "com.android.car.messenger.common.ACTION_DISMISS_NOTIFICATION";

    /** Used to mark a notification as read **/
    public static final String ACTION_MARK_AS_READ =
            "com.android.car.messenger.common.ACTION_MARK_AS_READ";

    /* EXTRAS */
    /** Key under which the {@link ConversationKey} is provided. */
    public static final String EXTRA_CONVERSATION_KEY =
            "com.android.car.messenger.common.EXTRA_CONVERSATION_KEY";

    /**
     * The resultKey of the {@link RemoteInput} which is sent in the reply callback {@link
     * Notification.Action}.
     */
    public static final String EXTRA_REMOTE_INPUT_KEY =
            "com.android.car.messenger.common.REMOTE_INPUT_KEY";

    protected final Context mContext;
    protected NotificationManager mNotificationManager;
    protected final boolean mUseLetterTile;

    /**
     * Maps a conversation's Notification Metadata to the conversation's unique key.
     * The extending class should always keep this map updated with the latest new/updated
     * notification information before calling {@link BaseNotificationDelegate#postNotification(
     * ConversationKey, ConversationNotificationInfo, String)}.
     **/
    protected final Map<ConversationKey, ConversationNotificationInfo> mNotificationInfos =
            new HashMap<>();

    /**
     * Maps a conversation's Notification Builder to the conversation's unique key. When the
     * conversation gets updated, this builder should be retrieved, updated, and reposted.
     **/
    private final Map<ConversationKey, NotificationCompat.Builder> mNotificationBuilders =
            new HashMap<>();

    /**
     * Maps a message's metadata with the message's unique key.
     * The extending class should always keep this map updated with the latest message information
     * before calling {@link BaseNotificationDelegate#postNotification(
     * ConversationKey, ConversationNotificationInfo, String)}.
     **/
    protected final Map<MessageKey, Message> mMessages = new HashMap<>();

    private final int mBitmapSize;
    private final float mCornerRadiusPercent;

    /**
     * Constructor for the BaseNotificationDelegate class.
     * @param context of the calling application.
     * @param useLetterTile whether a letterTile icon should be used if no avatar icon is given.
     **/
    public BaseNotificationDelegate(Context context, boolean useLetterTile) {
        mContext = context;
        mUseLetterTile = useLetterTile;
        mNotificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        mBitmapSize =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.notification_contact_photo_size);
        mCornerRadiusPercent = mContext.getResources()
                .getFloat(R.dimen.contact_avatar_corner_radius_percent);
    }

    /**
     * Removes all messages related to the inputted predicate, and cancels their notifications.
     **/
    public void cleanupMessagesAndNotifications(Predicate<CompositeKey> predicate) {
        clearNotifications(predicate);
        mNotificationBuilders.entrySet().removeIf(entry -> predicate.test(entry.getKey()));
        mNotificationInfos.entrySet().removeIf(entry -> predicate.test(entry.getKey()));
        mMessages.entrySet().removeIf(
                messageKeyMapMessageEntry -> predicate.test(messageKeyMapMessageEntry.getKey()));
    }

    /**
     * Clears all notifications matching the predicate. Example method calls are when user
     * wants to clear (a) message notification(s), or when the Bluetooth device that received the
     * messages has been disconnected.
     */
    public void clearNotifications(Predicate<CompositeKey> predicate) {
        mNotificationInfos.forEach((conversationKey, notificationInfo) -> {
            if (predicate.test(conversationKey)) {
                mNotificationManager.cancel(notificationInfo.getNotificationId());
            }
        });
    }

    protected void dismissInternal(ConversationKey convoKey) {
        clearNotifications(key -> key.equals(convoKey));
        excludeFromNotification(convoKey);
    }

    /**
     * Excludes messages from a notification so that the messages are not shown to the user once
     * the notification gets updated with newer messages.
     */
    protected void excludeFromNotification(ConversationKey convoKey) {
        ConversationNotificationInfo info = mNotificationInfos.get(convoKey);
        for (MessageKey key : info.mMessageKeys) {
            Message message = mMessages.get(key);
            message.excludeFromNotification();
        }
    }

    /**
     * Helper method to add {@link Message}s to the {@link ConversationNotificationInfo}. This
     * should be called when a new message has arrived.
     **/
    protected void addMessageToNotificationInfo(Message message, ConversationKey convoKey) {
        MessageKey messageKey = new MessageKey(message);
        boolean repeatMessage = mMessages.containsKey(messageKey);
        mMessages.put(messageKey, message);
        if (!repeatMessage) {
            ConversationNotificationInfo notificationInfo = mNotificationInfos.get(convoKey);
            notificationInfo.mMessageKeys.add(messageKey);
        }
    }

    /**
     * Creates a new notification, or updates an existing notification with the latest messages,
     * then posts it.
     * This should be called after the {@link ConversationNotificationInfo} object has been created,
     * and all of its {@link Message} objects have been linked to it.
     **/
    protected void postNotification(ConversationKey conversationKey,
            ConversationNotificationInfo notificationInfo, String channelId,
            @Nullable Bitmap avatarIcon) {
        boolean newNotification = !mNotificationBuilders.containsKey(conversationKey);

        NotificationCompat.Builder builder = newNotification ? new NotificationCompat.Builder(
                mContext, channelId) : mNotificationBuilders.get(
                conversationKey);
        builder.setChannelId(channelId);
        Message lastMessage = mMessages.get(notificationInfo.mMessageKeys.getLast());

        builder.setContentTitle(notificationInfo.getConvoTitle());
        builder.setContentText(mContext.getResources().getQuantityString(
                R.plurals.notification_new_message, notificationInfo.mMessageKeys.size(),
                notificationInfo.mMessageKeys.size()));

        if (avatarIcon != null) {
            builder.setLargeIcon(avatarIcon);
        } else if (mUseLetterTile) {
            BitmapDrawable drawable = (BitmapDrawable) TelecomUtils.createLetterTile(mContext,
                    Utils.getInitials(lastMessage.getSenderName(), ""),
                    lastMessage.getSenderName(), mBitmapSize, mCornerRadiusPercent)
                    .loadDrawable(mContext);
            builder.setLargeIcon(drawable.getBitmap());
        }
        // Else, no avatar icon will be shown.

        builder.setWhen(lastMessage.getReceivedTime());

        // Create MessagingStyle
        String userName = (notificationInfo.getUserDisplayName() == null
                || notificationInfo.getUserDisplayName().isEmpty()) ? mContext.getString(
                R.string.name_not_available) : notificationInfo.getUserDisplayName();
        Person user = new Person.Builder()
                .setName(userName)
                .build();
        NotificationCompat.MessagingStyle messagingStyle = new NotificationCompat.MessagingStyle(
                user);
        Person sender = new Person.Builder()
                .setName(lastMessage.getSenderName())
                .setUri(lastMessage.getSenderContactUri())
                .build();
        notificationInfo.mMessageKeys.stream().map(mMessages::get).forEachOrdered(message -> {
            if (!message.shouldExcludeFromNotification()) {
                messagingStyle.addMessage(
                        message.getMessageText(),
                        message.getReceivedTime(),
                        notificationInfo.isGroupConvo() ? new Person.Builder()
                                .setName(message.getSenderName())
                                .setUri(message.getSenderContactUri())
                                .build() : sender);
            }
        });
        if (notificationInfo.isGroupConvo()) {
            messagingStyle.setConversationTitle(Utils.constructGroupConversationHeader(
                    lastMessage.getSenderName(), notificationInfo.getConvoTitle(),
                    mContext.getString(R.string.group_conversation_title_separator)
            ));
        }

        // We are creating this notification for the first time.
        if (newNotification) {
            builder.setCategory(Notification.CATEGORY_MESSAGE);
            if (notificationInfo.getAppIcon() != null) {
                builder.setSmallIcon(IconCompat.createFromIcon(notificationInfo.getAppIcon()));
            } else {
                builder.setSmallIcon(R.drawable.ic_message);
            }

            builder.setShowWhen(true);
            messagingStyle.setGroupConversation(notificationInfo.isGroupConvo());

            if (notificationInfo.getAppDisplayName() != null) {
                Bundle displayName = new Bundle();
                displayName.putCharSequence(Notification.EXTRA_SUBSTITUTE_APP_NAME,
                        notificationInfo.getAppDisplayName());
                builder.addExtras(displayName);
            }

            PendingIntent deleteIntent = createServiceIntent(conversationKey,
                    notificationInfo.getNotificationId(),
                    ACTION_DISMISS_NOTIFICATION);
            builder.setDeleteIntent(deleteIntent);

            List<Action> actions = buildNotificationActions(conversationKey,
                    notificationInfo.getNotificationId());
            for (final Action action : actions) {
                builder.addAction(action);
            }
        }
        builder.setStyle(messagingStyle);

        mNotificationBuilders.put(conversationKey, builder);
        mNotificationManager.notify(notificationInfo.getNotificationId(), builder.build());
    }

    /** Can be overridden by any Delegates that have some devices that do not support reply. **/
    protected boolean shouldAddReplyAction(String deviceAddress) {
        return true;
    }

    private List<Action> buildNotificationActions(ConversationKey conversationKey,
            int notificationId) {
        final int icon = android.R.drawable.ic_media_play;

        final List<NotificationCompat.Action> actionList = new ArrayList<>();

        // Reply action
        if (shouldAddReplyAction(conversationKey.getDeviceId())) {
            final String replyString = mContext.getString(R.string.action_reply);
            PendingIntent replyIntent = createServiceIntent(conversationKey, notificationId,
                    ACTION_REPLY);
            actionList.add(
                    new NotificationCompat.Action.Builder(icon, replyString, replyIntent)
                            .setSemanticAction(NotificationCompat.Action.SEMANTIC_ACTION_REPLY)
                            .setShowsUserInterface(false)
                            .addRemoteInput(
                                    new androidx.core.app.RemoteInput.Builder(
                                            EXTRA_REMOTE_INPUT_KEY)
                                            .build()
                            )
                            .build()
            );
        }

        // Mark-as-read Action. This will be the callback of Notification Center's "Read" action.
        final String markAsRead = mContext.getString(R.string.action_mark_as_read);
        PendingIntent markAsReadIntent = createServiceIntent(conversationKey, notificationId,
                ACTION_MARK_AS_READ);
        actionList.add(
                new NotificationCompat.Action.Builder(icon, markAsRead, markAsReadIntent)
                        .setSemanticAction(NotificationCompat.Action.SEMANTIC_ACTION_MARK_AS_READ)
                        .setShowsUserInterface(false)
                        .build()
        );

        return actionList;
    }

    private PendingIntent createServiceIntent(ConversationKey conversationKey, int notificationId,
            String action) {
        Intent intent = new Intent(mContext, mContext.getClass())
                .setAction(action)
                .setClassName(mContext, mContext.getClass().getName())
                .putExtra(EXTRA_CONVERSATION_KEY, conversationKey);

        return PendingIntent.getForegroundService(mContext, notificationId, intent,
                PendingIntent.FLAG_UPDATE_CURRENT);
    }

}
