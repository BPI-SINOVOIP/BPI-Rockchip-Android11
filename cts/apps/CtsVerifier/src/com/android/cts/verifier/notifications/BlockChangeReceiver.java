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

package com.android.cts.verifier.notifications;

import static android.app.NotificationManager.ACTION_APP_BLOCK_STATE_CHANGED;
import static android.app.NotificationManager.ACTION_NOTIFICATION_CHANNEL_BLOCK_STATE_CHANGED;
import static android.app.NotificationManager.ACTION_NOTIFICATION_CHANNEL_GROUP_BLOCK_STATE_CHANGED;

import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

public class BlockChangeReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        SharedPreferences prefs = context.getSharedPreferences(
                NotificationListenerVerifierActivity.PREFS, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        if (ACTION_NOTIFICATION_CHANNEL_GROUP_BLOCK_STATE_CHANGED.equals(intent.getAction())) {
            String id = intent.getStringExtra(
                    NotificationManager.EXTRA_NOTIFICATION_CHANNEL_GROUP_ID);
            editor.putBoolean(id,
                    intent.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false));
        } else if (ACTION_NOTIFICATION_CHANNEL_BLOCK_STATE_CHANGED.equals(intent.getAction())) {
            String id = intent.getStringExtra(NotificationManager.EXTRA_NOTIFICATION_CHANNEL_ID);
            editor.putBoolean(id,
                    intent.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false));
        } else if (ACTION_APP_BLOCK_STATE_CHANGED.equals(intent.getAction())) {
            String id = context.getPackageName();
            editor.putBoolean(id,
                    intent.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false));
        }
        editor.commit();
    }
}
