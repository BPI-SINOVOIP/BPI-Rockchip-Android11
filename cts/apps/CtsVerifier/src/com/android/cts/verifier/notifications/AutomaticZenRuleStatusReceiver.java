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

import static android.app.NotificationManager.ACTION_AUTOMATIC_ZEN_RULE_STATUS_CHANGED;
import static android.app.NotificationManager.AUTOMATIC_RULE_STATUS_UNKNOWN;
import static android.app.NotificationManager.EXTRA_AUTOMATIC_ZEN_RULE_STATUS;

import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;

public class AutomaticZenRuleStatusReceiver extends BroadcastReceiver {
    private static final String TAG = "AZRReceiver";
    @Override
    public void onReceive(Context context, Intent intent) {
        SharedPreferences prefs = context.getSharedPreferences(
                ConditionProviderVerifierActivity.PREFS, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        if (ACTION_AUTOMATIC_ZEN_RULE_STATUS_CHANGED.equals(intent.getAction())) {
            String id = intent.getStringExtra(
                    NotificationManager.EXTRA_AUTOMATIC_ZEN_RULE_ID);
            int status = intent.getIntExtra(EXTRA_AUTOMATIC_ZEN_RULE_STATUS,
                    AUTOMATIC_RULE_STATUS_UNKNOWN);
            Log.d(TAG, "Got broadcast for rule status change: " + id + " " + status);
            editor.putInt(id, status);
            editor.commit();
        }
    }
}
