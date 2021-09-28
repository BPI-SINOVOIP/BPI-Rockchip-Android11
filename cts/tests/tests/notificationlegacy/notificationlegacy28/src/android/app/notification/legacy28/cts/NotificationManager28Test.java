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

package android.app.notification.legacy28.cts;

import static junit.framework.TestCase.assertEquals;
import static junit.framework.TestCase.assertNotNull;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Home for tests that need to verify behavior for apps that target old sdk versions.
 */
@RunWith(AndroidJUnit4.class)
public class NotificationManager28Test {
    final String TAG = "LegacyNoManTest28";

    final String NOTIFICATION_CHANNEL_ID = "LegacyNoManTest28";
    private NotificationManager mNotificationManager;
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();

        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mNotificationManager.createNotificationChannel(new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", NotificationManager.IMPORTANCE_DEFAULT));
    }

    @Test
    public void testPostFullScreenIntent_noPermission() {
        // No Full screen intent permission; but full screen intent should still be allowed
        int id = 6000;
        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(android.R.id.icon)
                        .setWhen(System.currentTimeMillis())
                        .setFullScreenIntent(getPendingIntent(), true)
                        .setContentText("This is #FSI notification")
                        .setContentIntent(getPendingIntent())
                        .build();
        mNotificationManager.notify(id, notification);

        StatusBarNotification n = findPostedNotification(id);
        assertNotNull(n);
        assertEquals(notification.fullScreenIntent, n.getNotification().fullScreenIntent);
    }

    private StatusBarNotification findPostedNotification(int id) {
        // notification is a bit asynchronous so it may take a few ms to appear in
        // getActiveNotifications()
        // we will check for it for up to 300ms before giving up
        StatusBarNotification n = null;
        for (int tries = 3; tries--> 0;) {
            final StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
            for (StatusBarNotification sbn : sbns) {
                Log.d(TAG, "Found " + sbn.getKey());
                if (sbn.getId() == id) {
                    n = sbn;
                    break;
                }
            }
            if (n != null) break;
            try {
                Thread.sleep(100);
            } catch (InterruptedException ex) {
                // pass
            }
        }
        return n;
    }

    private PendingIntent getPendingIntent() {
        return PendingIntent.getActivity(
                mContext, 0, new Intent(mContext, this.getClass()), 0);
    }
}
