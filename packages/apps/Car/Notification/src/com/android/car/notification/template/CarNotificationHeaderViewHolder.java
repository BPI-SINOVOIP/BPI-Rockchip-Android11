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
package com.android.car.notification.template;

import android.annotation.CallSuper;
import android.content.Context;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.android.car.notification.CarNotificationItemController;
import com.android.car.notification.R;

/**
 * Header template for the notification shade. This templates supports the clear all button with id
 * clear_all_button, a header text with id notification_header_text when the notification list is
 * not empty and a secondary header text with id empty_notification_header_text when notification
 * list is empty.
 */
public class CarNotificationHeaderViewHolder extends RecyclerView.ViewHolder {

    private final TextView mNotificationHeaderText;
    private final Button mClearAllButton;
    private final TextView mEmptyNotificationHeaderText;
    private final CarNotificationItemController mNotificationItemController;
    private final boolean mShowHeader;

    public CarNotificationHeaderViewHolder(Context context, View view,
            CarNotificationItemController notificationItemController) {
        super(view);

        if (notificationItemController == null) {
            throw new IllegalArgumentException(
                    "com.android.car.notification.template.CarNotificationHeaderViewHolder did not "
                            + "receive NotificationItemController from the Adapter.");
        }

        mNotificationHeaderText = view.findViewById(R.id.notification_header_text);
        mClearAllButton = view.findViewById(R.id.clear_all_button);
        mEmptyNotificationHeaderText = view.findViewById(R.id.empty_notification_header_text);
        mShowHeader = context.getResources().getBoolean(R.bool.config_showHeaderForNotifications);
        mNotificationItemController = notificationItemController;
    }

    @CallSuper
    public void bind(boolean containsNotification) {
        if (containsNotification && mShowHeader) {
            mNotificationHeaderText.setVisibility(View.VISIBLE);
            mEmptyNotificationHeaderText.setVisibility(View.GONE);

            if (mClearAllButton == null) {
                return;
            }

            mClearAllButton.setVisibility(View.VISIBLE);
            if (!mClearAllButton.hasOnClickListeners()) {
                mClearAllButton.setOnClickListener(view -> {
                    mNotificationItemController.clearAllNotifications();
                });
            }
            return;
        }

        if (containsNotification) {
            mEmptyNotificationHeaderText.setVisibility(View.GONE);
        } else {
            mEmptyNotificationHeaderText.setVisibility(View.VISIBLE);
        }
        mNotificationHeaderText.setVisibility(View.GONE);

        if (mClearAllButton != null) {
            mClearAllButton.setVisibility(View.GONE);
        }
    }
}
