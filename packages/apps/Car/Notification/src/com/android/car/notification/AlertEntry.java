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
import android.service.notification.StatusBarNotification;

import androidx.annotation.VisibleForTesting;

/**
 * Wrapper class to store the state of a {@link StatusBarNotification}.
 */
public class AlertEntry {

    private String mKey;
    private long mPostTime;
    private StatusBarNotification mStatusBarNotification;
    private NotificationClickHandlerFactory mClickHandlerFactory;

    public AlertEntry(StatusBarNotification statusBarNotification) {
        mStatusBarNotification = statusBarNotification;
        mKey = statusBarNotification.getKey();
        mPostTime = calculatePostTime();
    }

    // Empty constructor for Spy compatibility.
    @VisibleForTesting
    protected AlertEntry() {}

    /**
     * Updates the current post time for the Heads up notification.
     */
    void updatePostTime() {
        mPostTime = calculatePostTime();
    }

    long getPostTime() {
        return mPostTime;
    }

    /**
     * Calculate what the post time of a notification is at some current time.
     *
     * @return the post time
     */
    private long calculatePostTime() {
        return System.currentTimeMillis();
    }

    /**
     * Returns the {@link StatusBarNotification} that this instance of AlertEntry is wrapping.
     */
    public StatusBarNotification getStatusBarNotification() {
        return mStatusBarNotification;
    }

    NotificationClickHandlerFactory getClickHandlerFactory() {
        return mClickHandlerFactory;
    }

    void setClickHandlerFactory(NotificationClickHandlerFactory clickHandlerFactory) {
        mClickHandlerFactory = clickHandlerFactory;
    }

    /**
     * Returns the key associated with the {@link StatusBarNotification} stored in this AlertEntry
     * instance. This ensures that the same unique key is associated with a StatusBarNotification
     * and the {@link AlertEntry} that wraps it.
     */
    public String getKey() {
        return mKey;
    }

    /**
     * Returns the {@link Notification} that is associated with the {@link StatusBarNotification}
     * that this AlertEntry instance wraps.
     */
    public Notification getNotification() {
        return mStatusBarNotification.getNotification();
    }
}
