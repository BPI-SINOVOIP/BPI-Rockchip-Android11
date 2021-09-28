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

import static com.android.cts.forcestophelper.Constants.ACTION_ALARM;
import static com.android.cts.forcestophelper.Constants.EXTRA_ON_ALARM;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class AlarmReceiver extends BroadcastReceiver {

    private static final String TAG = AlarmReceiver.class.getSimpleName();

    @Override
    public void onReceive(Context context, Intent intent) {
        if (ACTION_ALARM.equals(intent.getAction())) {
            final PendingIntent onAlarm = intent.getParcelableExtra(EXTRA_ON_ALARM);
            if (onAlarm != null) {
                try {
                    onAlarm.send();
                } catch (PendingIntent.CanceledException e) {
                    Log.w(TAG, "onAlarm pending intent was canceled", e);
                }
            } else {
                Log.e(TAG, "Could not find pending intent extra " + EXTRA_ON_ALARM);
            }
        }
    }

    public static PendingIntent createAlarmPendingIntent(Context context, PendingIntent onAlarm) {
        final Intent alarmIntent = new Intent(ACTION_ALARM)
                .setClass(context, AlarmReceiver.class)
                .putExtra(EXTRA_ON_ALARM, onAlarm);
        return PendingIntent.getBroadcast(context, 0, alarmIntent,
                PendingIntent.FLAG_ONE_SHOT | PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
