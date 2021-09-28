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
 *
 */

package com.android.cts.forcestophelper;

import static com.android.cts.forcestophelper.Constants.ALARM_DELAY;
import static com.android.cts.forcestophelper.Constants.EXTRA_ON_ALARM;
import static com.android.cts.forcestophelper.Constants.EXTRA_ON_TASK_REMOVED;
import static com.android.cts.forcestophelper.AlarmReceiver.createAlarmPendingIntent;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

/**
 * Service that listens for {@link #onTaskRemoved(Intent)} for any task in this application.
 */
public class TaskRemovedListenerService extends Service {
    private static final String TAG = TaskRemovedListenerService.class.getSimpleName();

    private PendingIntent mOnTaskRemoved;
    private PendingIntent mOnAlarm;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        mOnTaskRemoved = intent.getParcelableExtra(EXTRA_ON_TASK_REMOVED);
        mOnAlarm = intent.getParcelableExtra(EXTRA_ON_ALARM);

        if (mOnTaskRemoved != null && mOnAlarm != null) {
            final NotificationManager nm = getSystemService(NotificationManager.class);
            nm.createNotificationChannel(
                    new NotificationChannel(TAG, TAG, NotificationManager.IMPORTANCE_DEFAULT));
            final Notification notification = new Notification.Builder(this, TAG)
                    .setSmallIcon(R.drawable.cts_verifier)
                    .setContentTitle("Test Service")
                    .build();
            startForeground(1, notification);
        } else {
            Log.e(TAG, "Need pending intents for onAlarm and onTaskRemoved. Stopping service.");
            stopSelf();
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mOnTaskRemoved != null) {
            Log.e(TAG, "Stopping without delivering mOnTaskRemoved");
        }
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        super.onTaskRemoved(rootIntent);
        if (mOnTaskRemoved != null) {
            try {
                mOnTaskRemoved.send();
            } catch (PendingIntent.CanceledException e) {
                Log.w(TAG, "mOnTaskRemoved was canceled", e);
            }
            mOnTaskRemoved = null;
        }
        final PendingIntent alarmPi = createAlarmPendingIntent(this, mOnAlarm);
        final AlarmManager am = getSystemService(AlarmManager.class);
        am.setExactAndAllowWhileIdle(AlarmManager.ELAPSED_REALTIME_WAKEUP, ALARM_DELAY, alarmPi);

        stopSelf();
    }
}
