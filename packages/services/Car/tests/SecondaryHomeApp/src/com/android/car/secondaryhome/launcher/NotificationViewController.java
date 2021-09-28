/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.car.secondaryhome.launcher;

import android.app.Notification;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.widget.TextView;

import com.android.car.notification.AlertEntry;
import com.android.car.notification.PreprocessingManager;
import com.android.car.secondaryhome.R;

/**
 * This object will handle updating notification view.
 * Each Notification should be shown on CarUiRecyclerView.
 */
public class NotificationViewController {
    private static final String TAG = "NotificationViewControl";
    private final View mCarNotificationView;
    private final NotificationListener mCarNotificationListener;
    private final NotificationUpdateHandler mNotificationUpdateHandler =
            new NotificationUpdateHandler();
    private final PreprocessingManager mPreprocessingManager;
    private final TextView mTextBox;

    public NotificationViewController(View carNotificationView,
            PreprocessingManager preprocessingManager,
            NotificationListener carNotificationListener) {
        mCarNotificationView = carNotificationView;
        mCarNotificationListener = carNotificationListener;
        mPreprocessingManager = preprocessingManager;
        mTextBox = mCarNotificationView.findViewById(R.id.debug);
        resetNotifications();
    }

    /**
     * Set handler to NotificationListener
     */
    public void enable() {
        if (mCarNotificationListener != null) {
            mCarNotificationListener.setHandler(mNotificationUpdateHandler);
        }
    }

    /**
     * Remove handler
     */
    public void disable() {
        if (mCarNotificationListener != null) {
            mCarNotificationListener.setHandler(null);
        }
    }

    /**
     * Update notifications: print to text box
     */
    private void updateNotifications(int what, AlertEntry alertEntry) {
        // filter out navigation notification, otherwise it will spam
        if (Notification.CATEGORY_NAVIGATION.equals(
                    alertEntry.getNotification().category)) {
            return;
        }

        mTextBox.append("New: " + alertEntry.getStatusBarNotification() + "\n");
    }

    /**
    * Reset notifications to the latest state.
    */
    public void resetNotifications() {
        mPreprocessingManager.init(
                mCarNotificationListener.getNotifications(),
                mCarNotificationListener.getCurrentRanking());

        mPreprocessingManager.process(
                true /* showLessImportantNotifications */,
                mCarNotificationListener.getNotifications(),
                mCarNotificationListener.getCurrentRanking());
        mTextBox.setText("");
    }

    private class NotificationUpdateHandler extends Handler {
        @Override
        public void handleMessage(Message message) {
            updateNotifications(message.what, (AlertEntry) message.obj);
        }
    }
}
