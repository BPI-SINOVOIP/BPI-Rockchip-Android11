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

package com.android.test.notificationdelegator;

import static android.app.NotificationManager.IMPORTANCE_LOW;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.os.Bundle;
import android.util.Log;

public class NotificationDelegateAndPost extends Activity {
    private static final String TAG = "DelegateAndPost";
    private static final String DELEGATE = "android.app.stubs";
    private static final String CHANNEL = "channel";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity);

        NotificationManager nm = getSystemService(NotificationManager.class);

        nm.createNotificationChannel(new NotificationChannel(CHANNEL, CHANNEL, IMPORTANCE_LOW));
        nm.setNotificationDelegate(DELEGATE);
        Log.d(TAG, "Set delegate: " + nm.getNotificationDelegate());

        Log.d(TAG, "Posting notification with id 9");

        Notification n = new Notification.Builder(this, CHANNEL)
                .setSmallIcon(android.R.drawable.sym_def_app_icon)
                .setContentTitle("posted by delegator")
                .build();

        nm.notify(9, n);

        finish();
    }
}
