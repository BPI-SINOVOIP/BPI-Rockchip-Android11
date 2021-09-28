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

import android.app.Notification;
import android.os.Bundle;
import android.os.Handler;
import android.service.notification.StatusBarNotification;
import android.view.View;

import com.android.car.notification.template.CarNotificationBaseViewHolder;

/**
 * Class to store the state for Heads Up Notifications. Each notification will have its own post
 * time, handler, and Layout. This class ensures to store it as a separate state so that each Heads
 * up notification can be controlled independently.
 */
public class HeadsUpEntry extends AlertEntry {

    private final Handler mHandler;
    private View mNotificationView;
    private CarNotificationBaseViewHolder mCarNotificationBaseViewHolder;

    boolean mIsAlertAgain;
    boolean mIsNewHeadsUp;

    HeadsUpEntry(StatusBarNotification statusBarNotification) {
        super(statusBarNotification);
        mHandler = new Handler();
    }

    /**
     * Handler will use the method {@link Handler#postDelayed(Runnable, long)} which will control
     * the dismiss time for the Heads Up notification. All the notifications should have their own
     * handler to control this time individually.
     */
    Handler getHandler() {
        return mHandler;
    }

    /**
     * View that holds the actual card for heads up notification.
     */
    void setNotificationView(View notificationView) {
        mNotificationView = notificationView;
    }

    View getNotificationView() {
        return mNotificationView;
    }

    void setViewHolder(CarNotificationBaseViewHolder viewHolder) {
        mCarNotificationBaseViewHolder = viewHolder;
    }

    CarNotificationBaseViewHolder getViewHolder() {
        return mCarNotificationBaseViewHolder;
    }
}
