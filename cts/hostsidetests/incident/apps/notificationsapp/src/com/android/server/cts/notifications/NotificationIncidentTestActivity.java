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

package com.android.server.cts.notifications;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;

public class NotificationIncidentTestActivity extends Activity {
    final String TAG = "NotificationIncidentTestActivity";
    final String NOTIFICATION_CHANNEL_ID = "LegacyNotificationManagerTest";
    final String NOTIFICATION_CHANNEL_NAME = "LegacyNotificationManagerTest";

    @Override
    public void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        Context context = this;
        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.createNotificationChannel(
                new NotificationChannel(
                        NOTIFICATION_CHANNEL_ID,
                        NOTIFICATION_CHANNEL_NAME,
                        NotificationManager.IMPORTANCE_DEFAULT));

        final Notification notification =
                new Notification.Builder(context, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(R.drawable.icon_black)
                        .setContentTitle("notify")
                        .setContentText("This is notification  ")
                        .build();
        notificationManager.notify(1, notification);

        Log.i(TAG, "Notification posted.");
    }
}
