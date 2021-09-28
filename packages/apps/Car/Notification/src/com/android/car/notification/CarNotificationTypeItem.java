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

package com.android.car.notification;

import android.view.View;

import com.android.car.notification.template.BasicNotificationViewHolder;
import com.android.car.notification.template.CallNotificationViewHolder;
import com.android.car.notification.template.CarNotificationBaseViewHolder;
import com.android.car.notification.template.EmergencyNotificationViewHolder;
import com.android.car.notification.template.GroupNotificationViewHolder;
import com.android.car.notification.template.GroupSummaryNotificationViewHolder;
import com.android.car.notification.template.InboxNotificationViewHolder;
import com.android.car.notification.template.MessageNotificationViewHolder;
import com.android.car.notification.template.NavigationNotificationViewHolder;
import com.android.car.notification.template.ProgressNotificationViewHolder;

import java.util.HashMap;
import java.util.Map;

/**
 * Enum for storing the templates and view type for a particular notification.
 */
public enum CarNotificationTypeItem {
    EMERGENCY(R.layout.car_emergency_headsup_notification_template,
            R.layout.car_emergency_notification_template, NotificationViewType.CAR_EMERGENCY,
            false),
    NAVIGATION(R.layout.navigation_headsup_notification_template,
            -1, NotificationViewType.NAVIGATION, false),
    CALL(R.layout.call_headsup_notification_template,
            R.layout.call_notification_template, NotificationViewType.CALL, false),
    WARNING(R.layout.car_warning_headsup_notification_template,
            R.layout.car_warning_notification_template, NotificationViewType.CAR_WARNING, false),
    INFORMATION(R.layout.car_information_headsup_notification_template,
            R.layout.car_information_notification_template, NotificationViewType.CAR_INFORMATION,
            false),
    INFORMATION_IN_GROUP(-1, R.layout.car_information_notification_template_inner,
            NotificationViewType.CAR_INFORMATION_IN_GROUP, true),
    MESSAGE(R.layout.message_headsup_notification_template,
            R.layout.message_notification_template, NotificationViewType.MESSAGE, false),
    MESSAGE_IN_GROUP(-1, R.layout.message_notification_template_inner,
            NotificationViewType.MESSAGE_IN_GROUP, true),
    INBOX(R.layout.inbox_headsup_notification_template,
            R.layout.inbox_notification_template, NotificationViewType.INBOX, false),
    INBOX_IN_GROUP(-1, R.layout.inbox_notification_template_inner,
            NotificationViewType.INBOX_IN_GROUP, true),
    PROGRESS(-1, R.layout.progress_notification_template, NotificationViewType.PROGRESS, false),
    PROGRESS_IN_GROUP(-1, R.layout.progress_notification_template_inner,
            NotificationViewType.PROGRESS_IN_GROUP, true),
    BASIC(R.layout.basic_headsup_notification_template,
            R.layout.basic_notification_template, NotificationViewType.BASIC, false),
    BASIC_IN_GROUP(-1, R.layout.basic_notification_template_inner,
            NotificationViewType.BASIC_IN_GROUP, true),
    GROUP_EXPANDED(-1, R.layout.group_notification_template, NotificationViewType.GROUP_EXPANDED,
            false),
    GROUP_COLLAPSED(-1, R.layout.group_notification_template, NotificationViewType.GROUP_COLLAPSED,
            false),
    GROUP_SUMMARY(-1, R.layout.group_summary_notification_template,
            NotificationViewType.GROUP_SUMMARY, false);

    private final int mHeadsUpTemplate;
    private final int mNotificationCenterTemplate;
    private final int mNotificationType;
    private final boolean mIsInGroup;

    private static final Map<Integer, CarNotificationTypeItem>
            VIEW_TYPE_CAR_NOTIFICATION_TYPE_ITEM_MAP = new HashMap<>(
            values().length, /* loadFactor= */ 1);

    static {
        for (CarNotificationTypeItem carNotificationTypeItem : values()) {
            VIEW_TYPE_CAR_NOTIFICATION_TYPE_ITEM_MAP.put(
                    carNotificationTypeItem.getNotificationType(), carNotificationTypeItem);
        }
    }

    CarNotificationTypeItem(int headsUpTemplate, int notificationCenterTemplate,
            int notificationType, boolean isInGroup) {
        mHeadsUpTemplate = headsUpTemplate;
        mNotificationCenterTemplate = notificationCenterTemplate;
        mNotificationType = notificationType;
        mIsInGroup = isInGroup;
    }

    /**
     * Return the heads up notification template id defined in enum. If no template is defined this
     * will return -1.
     */
    public int getHeadsUpTemplate() {
        return mHeadsUpTemplate;
    }

    /**
     * Return the notification center template id defined in enum. If no template is defined this
     * will return -1.
     */
    public int getNotificationCenterTemplate() {
        return mNotificationCenterTemplate;
    }

    /**
     * Returns the notification type defined for this enum.
     */
    public int getNotificationType() {
        return mNotificationType;
    }

    /**
     * Returns the {@link CarNotificationBaseViewHolder} based on the view type defined in enum.
     */
    public CarNotificationBaseViewHolder getViewHolder(View view,
            NotificationClickHandlerFactory clickHandlerFactory) {
        switch (mNotificationType) {
            case NotificationViewType.CAR_EMERGENCY:
                return new EmergencyNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.NAVIGATION:
                return new NavigationNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.CALL:
                return new CallNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.MESSAGE:
            case NotificationViewType.MESSAGE_IN_GROUP:
                return new MessageNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.INBOX:
            case NotificationViewType.INBOX_IN_GROUP:
                return new InboxNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.GROUP_EXPANDED:
            case NotificationViewType.GROUP_COLLAPSED:
                return new GroupNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.GROUP_SUMMARY:
                return new GroupSummaryNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.PROGRESS:
            case NotificationViewType.PROGRESS_IN_GROUP:
                return new ProgressNotificationViewHolder(view, clickHandlerFactory);
            case NotificationViewType.BASIC:
            case NotificationViewType.BASIC_IN_GROUP:
            case NotificationViewType.CAR_WARNING:
            case NotificationViewType.CAR_INFORMATION:
            case NotificationViewType.CAR_INFORMATION_IN_GROUP:
                return new BasicNotificationViewHolder(view, clickHandlerFactory);
            default:
                throw new AssertionError("invalid notification type" + mNotificationType);
        }
    }

    /**
     * Binds a {@link AlertEntry} to a notification template.
     */
    public void bind(AlertEntry alertEntry, boolean isHeadsUp,
            CarNotificationBaseViewHolder holder) {
        holder.bind(alertEntry, mIsInGroup, isHeadsUp);
    }

    /**
     * Returns the enum object based on the view type defined in the enum.
     */
    public static CarNotificationTypeItem of(int viewType) {
        CarNotificationTypeItem result = VIEW_TYPE_CAR_NOTIFICATION_TYPE_ITEM_MAP.get(viewType);
        if (result == null) {
            throw new IllegalArgumentException("Invalid view type id: " + viewType);
        }
        return result;
    }
}
