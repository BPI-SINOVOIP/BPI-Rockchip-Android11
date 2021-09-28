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

package android.app.stubs;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.util.Log;

/**
 * Foreground Service with location type.
 */
public class LocalForegroundServiceLocation extends LocalForegroundService {

    private static final String TAG = "LocalForegroundServiceLocation";
    private static final String NOTIFICATION_CHANNEL_ID = "cts/" + TAG;
    public static final String EXTRA_FOREGROUND_SERVICE_TYPE = "ForegroundService.type";
    public static final int COMMAND_START_FOREGROUND_WITH_TYPE = 1;
    public static String ACTION_START_FGSL_RESULT =
            "android.app.stubs.LocalForegroundServiceLocation.RESULT";
    private int mNotificationId = 10;

    /** Returns the channel id for this service */
    @Override
    protected String getNotificationChannelId() {
        return NOTIFICATION_CHANNEL_ID;
    }

    @Override
    public void onStart(Intent intent, int startId) {
        String notificationChannelId = getNotificationChannelId();
        NotificationManager notificationManager = getSystemService(NotificationManager.class);
        notificationManager.createNotificationChannel(new NotificationChannel(
            notificationChannelId, notificationChannelId,
            NotificationManager.IMPORTANCE_DEFAULT));

        Context context = getApplicationContext();
        int command = intent.getIntExtra(EXTRA_COMMAND, -1);
        switch (command) {
            case COMMAND_START_FOREGROUND_WITH_TYPE:
                final int type = intent.getIntExtra(EXTRA_FOREGROUND_SERVICE_TYPE,
                        ServiceInfo.FOREGROUND_SERVICE_TYPE_MANIFEST);
                mNotificationId ++;
                final Notification notification =
                    new Notification.Builder(context, NOTIFICATION_CHANNEL_ID)
                        .setContentTitle(getNotificationTitle(mNotificationId))
                        .setSmallIcon(R.drawable.black)
                        .build();
                startForeground(mNotificationId, notification);
                //assertEquals(type, getForegroundServiceType());
                break;
            default:
                Log.e(TAG, "Unknown command: " + command);
        }

        sendBroadcast(new Intent(ACTION_START_FGSL_RESULT)
                .setFlags(Intent.FLAG_RECEIVER_FOREGROUND));
    }
}
