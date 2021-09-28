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

package android.app.cts.activitymanager.api29;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

public class LocationForegroundService extends Service {
    private static final String NOTIFICATION_CHANNEL_ID =
            LocationForegroundService.class.getSimpleName();

    public static String ACTION_SERVICE_START_RESULT =
            "android.app.cts.activitymanager.api29.LocationForegroundService.RESULT";

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        getSystemService(NotificationManager.class).createNotificationChannel(
                new NotificationChannel(NOTIFICATION_CHANNEL_ID, NOTIFICATION_CHANNEL_ID,
                NotificationManager.IMPORTANCE_DEFAULT));
        Notification status = new Notification.Builder(this, NOTIFICATION_CHANNEL_ID)
                .setContentTitle("LocalForegroundService running")
                .setSmallIcon(R.drawable.black)
                .build();
        startForeground(1, status);
        sendBroadcast(
                new Intent(ACTION_SERVICE_START_RESULT).setFlags(Intent.FLAG_RECEIVER_FOREGROUND));
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO Auto-generated method stub
        return null;
    }

}

