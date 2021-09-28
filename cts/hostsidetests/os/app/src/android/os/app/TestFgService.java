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

package android.os.app;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Process;
import android.util.Log;

public class TestFgService extends Service {
    private static final String TAG = "TestFgService";

    // intentionally invalid resource configuration
    private static final int NOTIFICATION_ID = 5038;
    private static final String CHANNEL = "fgservice";

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "onStartCommand() called");
        Notification notification = new Notification.Builder(this, CHANNEL)
                .setContentTitle("Foreground service")
                .setContentText("Ongoing test app foreground service is live")
                .setSmallIcon(NOTIFICATION_ID)
                .build();

        Log.i(TAG, "TestFgService starting foreground: pid=" + Process.myPid());
        startForeground(NOTIFICATION_ID, notification);

        return START_NOT_STICKY;
    }
}