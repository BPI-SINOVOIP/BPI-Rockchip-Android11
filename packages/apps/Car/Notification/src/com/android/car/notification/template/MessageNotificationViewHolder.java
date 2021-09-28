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
package com.android.car.notification.template;

import android.annotation.ColorInt;
import android.app.Notification;
import android.app.Person;
import android.content.Context;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.Parcelable;
import android.text.TextUtils;
import android.view.View;
import android.widget.DateTimeView;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.app.NotificationCompat.MessagingStyle;

import com.android.car.notification.AlertEntry;
import com.android.car.notification.NotificationClickHandlerFactory;
import com.android.car.notification.PreprocessingManager;
import com.android.car.notification.R;

import java.util.List;

/**
 * Messaging notification template that displays a messaging notification and a voice reply button.
 */
public class MessageNotificationViewHolder extends CarNotificationBaseViewHolder {
    @ColorInt
    private final int mDefaultPrimaryForegroundColor;
    private final Context mContext;
    private final CarNotificationBodyView mBodyView;
    private final CarNotificationHeaderView mHeaderView;
    private final CarNotificationActionsView mActionsView;
    private final TextView mTitleView;
    private final DateTimeView mTimeView;
    private final TextView mMessageView;
    private final TextView mUnshownCountView;
    private final ImageView mAvatarView;
    private NotificationClickHandlerFactory mClickHandlerFactory;

    public MessageNotificationViewHolder(
            View view, NotificationClickHandlerFactory clickHandlerFactory) {
        super(view, clickHandlerFactory);
        mContext = view.getContext();
        mDefaultPrimaryForegroundColor = mContext.getColor(R.color.primary_text_color);
        mHeaderView = view.findViewById(R.id.notification_header);
        mActionsView = view.findViewById(R.id.notification_actions);
        mTitleView = view.findViewById(R.id.notification_body_title);
        mTimeView = view.findViewById(R.id.in_group_time_stamp);
        if (mTimeView != null) {
            // HUN template does not include the time stamp.
            mTimeView.setShowRelativeTime(true);
        }
        mMessageView = view.findViewById(R.id.notification_body_content);
        mBodyView = view.findViewById(R.id.notification_body);
        mUnshownCountView = view.findViewById(R.id.message_count);
        mAvatarView = view.findViewById(R.id.notification_body_icon);
        mClickHandlerFactory = clickHandlerFactory;
    }

    /**
     * Binds a {@link AlertEntry} to a messaging car notification template without
     * UX restriction.
     */
    @Override
    public void bind(AlertEntry alertEntry, boolean isInGroup,
            boolean isHeadsUp) {
        super.bind(alertEntry, isInGroup, isHeadsUp);
        bindBody(alertEntry, isInGroup, /* isRestricted= */ false, isHeadsUp);
        mHeaderView.bind(alertEntry, isInGroup);
        mActionsView.bind(mClickHandlerFactory, alertEntry);
    }

    /**
     * Binds a {@link AlertEntry} to a messaging car notification template with
     * UX restriction.
     */
    public void bindRestricted(AlertEntry alertEntry, boolean isInGroup, boolean isHeadsUp) {
        super.bind(alertEntry, isInGroup, isHeadsUp);
        bindBody(alertEntry, isInGroup, /* isRestricted= */ true, isHeadsUp);
        mHeaderView.bind(alertEntry, isInGroup);
        mActionsView.bind(mClickHandlerFactory, alertEntry);
    }

    /**
     * Private method that binds the data to the view.
     */
    private void bindBody(AlertEntry alertEntry, boolean isInGroup, boolean isRestricted,
            boolean isHeadsUp) {

        Notification notification = alertEntry.getNotification();
        CharSequence messageText = null;
        CharSequence conversationTitle = null;
        Icon avatar = null;
        Integer messageCount = null;

        final MessagingStyle messagingStyle =
                MessagingStyle.extractMessagingStyleFromNotification(notification);
        if (messagingStyle != null) conversationTitle = messagingStyle.getConversationTitle();

        Bundle extras = notification.extras;
        Parcelable[] messagesData = extras.getParcelableArray(Notification.EXTRA_MESSAGES);
        if (messagesData != null) {
            List<Notification.MessagingStyle.Message> messages =
                    Notification.MessagingStyle.Message.getMessagesFromBundleArray(messagesData);
            if (messages != null && !messages.isEmpty()) {
                messageCount = messages.size();
                // Use the latest message
                Notification.MessagingStyle.Message message = messages.get(messages.size() - 1);
                messageText = message.getText();
                Person sender = message.getSenderPerson();
                if (sender != null) {
                    avatar = sender.getIcon();
                }
                if (conversationTitle == null) {
                    conversationTitle = sender != null ? sender.getName() : message.getSender();
                }
            }
        }

        // app did not use messaging style, fall back to standard fields
        if (messageCount == null) {
            messageCount = notification.number;
            if (messageCount == 0) {
                messageCount = 1; // a notification should at least represent 1 message
            }
        }

        if (TextUtils.isEmpty(conversationTitle)) {
            conversationTitle = extras.getCharSequence(Notification.EXTRA_TITLE);
        }
        if (isRestricted) {
            messageText = mContext.getResources().getQuantityString(
                    R.plurals.restricted_message_text, messageCount, messageCount);
        } else if (TextUtils.isEmpty(messageText)) {
            messageText = extras.getCharSequence(Notification.EXTRA_TEXT);
        }

        if (avatar == null) {
            avatar = notification.getLargeIcon();
        }

        if (!TextUtils.isEmpty(conversationTitle)) {
            mTitleView.setVisibility(View.VISIBLE);
            mTitleView.setText(conversationTitle);
        }

        if (isInGroup && notification.showsTime()) {
            mTimeView.setVisibility(View.VISIBLE);
            mTimeView.setTime(notification.when);
        }

        if (!TextUtils.isEmpty(messageText)) {
            messageText = PreprocessingManager.getInstance(mContext).trimText(messageText);
            mMessageView.setVisibility(View.VISIBLE);
            mMessageView.setText(messageText);
        }

        if (avatar != null) {
            mAvatarView.setVisibility(View.VISIBLE);
            mAvatarView.setImageIcon(avatar);
        }

        int unshownCount = messageCount - 1;
        if (!isRestricted && unshownCount > 0) {
            String unshownCountText =
                    mContext.getString(R.string.message_unshown_count, unshownCount);
            mUnshownCountView.setVisibility(View.VISIBLE);
            mUnshownCountView.setText(unshownCountText);
            mUnshownCountView.setTextColor(getAccentColor());
        }

        if (isHeadsUp) {
            mBodyView.bindTitleAndMessage(conversationTitle, messageText);
        }
    }

    @Override
    void reset() {
        super.reset();
        mTitleView.setVisibility(View.GONE);
        mTitleView.setText(null);
        if (mTimeView != null) {
            mTimeView.setVisibility(View.GONE);
        }

        mMessageView.setVisibility(View.GONE);
        mMessageView.setText(null);

        mAvatarView.setVisibility(View.GONE);
        mAvatarView.setImageIcon(null);

        mUnshownCountView.setVisibility(View.GONE);
        mUnshownCountView.setText(null);
        mUnshownCountView.setTextColor(mDefaultPrimaryForegroundColor);
    }
}
