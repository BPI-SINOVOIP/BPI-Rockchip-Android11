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

import androidx.recyclerview.widget.RecyclerView;

import com.android.car.notification.CarNotificationItemController;
import com.android.car.notification.R;

/**
 * Footer template for the notification shade. This templates supports the clear all button with id
 * clear_all_button.
 */
public class CarNotificationFooterViewHolder extends RecyclerView.ViewHolder {

    private final Button mClearAllButton;
    private final CarNotificationItemController mNotificationItemController;
    private final boolean mShowFooter;

    public CarNotificationFooterViewHolder(Context context, View view,
            CarNotificationItemController notificationItemController) {
        super(view);

        if (notificationItemController == null) {
            throw new IllegalArgumentException(
                    "com.android.car.notification.template.CarNotificationFooterViewHolder did not "
                            + "receive NotificationItemController from the Adapter.");
        }

        mShowFooter = context.getResources().getBoolean(R.bool.config_showFooterForNotifications);
        mClearAllButton = view.findViewById(R.id.clear_all_button);
        mNotificationItemController = notificationItemController;
    }

    @CallSuper
    public void bind(boolean containsNotification) {
        if (mClearAllButton == null || !mShowFooter) {
            return;
        }

        if (containsNotification) {
            mClearAllButton.setVisibility(View.VISIBLE);
            if (!mClearAllButton.hasOnClickListeners()) {
                mClearAllButton.setOnClickListener(view -> {
                    mNotificationItemController.clearAllNotifications();
                });
            }
            return;
        }
        mClearAllButton.setVisibility(View.GONE);
    }
}
