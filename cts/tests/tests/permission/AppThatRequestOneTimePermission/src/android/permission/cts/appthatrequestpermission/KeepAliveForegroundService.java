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

package android.permission.cts.appthatrequestpermission;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

public class KeepAliveForegroundService extends Service {

    private static final String EXTRA_FOREGROUND_SERVICE_LIFESPAN =
            "android.permission.cts.OneTimePermissionTest.EXTRA_FOREGROUND_SERVICE_LIFESPAN";

    private static final String EXTRA_FOREGROUND_SERVICE_STICKY =
            "android.permission.cts.OneTimePermissionTest.EXTRA_FOREGROUND_SERVICE_STICKY";

    private static final String CHANNEL_ID = "channelId";
    private static final String CHANNEL_NAME = "channelName";

    private static final long DEFAULT_LIFESPAN = 5000;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        long lifespan;
        boolean sticky;
        if (intent == null) {
            lifespan = DEFAULT_LIFESPAN;
            sticky = false;
        } else {
            lifespan = intent.getLongExtra(EXTRA_FOREGROUND_SERVICE_LIFESPAN, DEFAULT_LIFESPAN);
            sticky = intent.getBooleanExtra(EXTRA_FOREGROUND_SERVICE_STICKY, false);
        }
        NotificationManager notificationManager = getSystemService(NotificationManager.class);
        notificationManager.createNotificationChannel(
                new NotificationChannel(CHANNEL_ID, CHANNEL_NAME,
                        NotificationManager.IMPORTANCE_LOW));
        Notification notification = new Notification.Builder(this, CHANNEL_ID)
                .setSmallIcon(android.R.drawable.ic_lock_lock)
                .build();
        startForeground(1, notification);
        new Handler(Looper.getMainLooper()).postDelayed(
                () -> stopForeground(Service.STOP_FOREGROUND_REMOVE), lifespan);
        if (sticky) {
            return START_STICKY;
        }
        return super.onStartCommand(intent, flags, startId);
    }
}
