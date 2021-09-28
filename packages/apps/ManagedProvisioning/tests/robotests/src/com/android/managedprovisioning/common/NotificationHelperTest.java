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
 * limitations under the License
 */

package com.android.managedprovisioning.common;

import static com.android.managedprovisioning.common.NotificationHelper.CHANNEL_ID;
import static com.android.managedprovisioning.common.NotificationHelper.ENCRYPTION_NOTIFICATION_ID;
import static com.android.managedprovisioning.common.NotificationHelper.PRIVACY_REMINDER_NOTIFICATION_ID;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.service.notification.StatusBarNotification;

import com.android.managedprovisioning.R;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(RobolectricTestRunner.class)
public class NotificationHelperTest {

    private Context mContext = RuntimeEnvironment.application;
    private NotificationManager mNotificationManager =
            mContext.getSystemService(NotificationManager.class);

    private NotificationHelper mNotificationHelper = new NotificationHelper(mContext);

    @Test
    public void showResumeNotification_showsExpectedNotification() {
        final Intent intent = new Intent(Globals.ACTION_RESUME_PROVISIONING);
        mNotificationHelper.showResumeNotification(intent);

        assertThat(shadowOf(mNotificationManager).getActiveNotifications()).hasLength(1);
        final StatusBarNotification notification =
                shadowOf(mNotificationManager).getActiveNotifications()[0];
        assertThat(notification.getId()).isEqualTo(ENCRYPTION_NOTIFICATION_ID);
        assertThat(notification.getNotification().getChannelId()).isEqualTo(CHANNEL_ID);
        assertThat(notification.getNotification().extras.getString(Notification.EXTRA_TITLE))
                .isEqualTo(mContext.getString(R.string.continue_provisioning_notify_title));
    }

    @Test
    public void showPrivacyReminderNotification_showsExpectedNotification() {
        mNotificationHelper.showPrivacyReminderNotification(
                mContext, NotificationManager.IMPORTANCE_DEFAULT);

        assertThat(shadowOf(mNotificationManager).getActiveNotifications()).hasLength(1);
        final StatusBarNotification notification =
                shadowOf(mNotificationManager).getActiveNotifications()[0];
        assertThat(notification.getId()).isEqualTo(PRIVACY_REMINDER_NOTIFICATION_ID);
        assertThat(notification.getNotification().getChannelId()).isEqualTo(CHANNEL_ID);
        assertThat(notification.getNotification().extras.getString(Notification.EXTRA_TITLE))
                .isEqualTo(mContext.getString(
                        R.string.fully_managed_device_provisioning_privacy_title));
    }
}
