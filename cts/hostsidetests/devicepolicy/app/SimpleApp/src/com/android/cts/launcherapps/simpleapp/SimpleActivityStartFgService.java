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

package com.android.cts.launcherapps.simpleapp;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

/**
 * Test being able to start a service (with no background check restrictions) as soon as
 * an activity is created.
 */
public class SimpleActivityStartFgService extends Activity {
    private static final String TAG = "SimpleActivityStartFgService";

    static final String ACTION_START_THEN_FG =
            "com.android.cts.launcherapps.simpleapp.SimpleActivityStartFgService.START_THEN_FG";
    public static String ACTION_FINISH_EVERYTHING =
            "com.android.cts.launcherapps.simpleapp.SimpleActivityStartFgService.FINISH_ALL";
    public static String ACTION_SIMPLE_ACTIVITY_START_FG_SERVICE_RESULT =
            "com.android.cts.launcherapps.simpleapp.SimpleActivityStartFgService.NOW_FOREGROUND";

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        final String action = getIntent().getAction();
        Log.i(TAG, "onCreate() with intent " + action);
        if (ACTION_START_THEN_FG.equals(action)) {
            if (!attemptStartService()) {
                // If we couldn't run the test properly, finish immediately
                Log.i(TAG, "   Couldn't start service, finishing immediately!");
                finish();
            }
        } else if (ACTION_FINISH_EVERYTHING.equals(action)) {
            Log.i(TAG, "   *** told to stop service & finish");
            Intent serviceIntent = getIntent().getParcelableExtra("service");
            stopService(serviceIntent);
            finish();
        }
    }

    @Override
    public void onStart() {
        super.onStart();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        final String action = intent.getAction();
        if (ACTION_FINISH_EVERYTHING.equals(action)) {
            Intent serviceIntent = getIntent().getParcelableExtra("service");
            stopService(serviceIntent);
            finish();
        }
    }

    private boolean attemptStartService() {
        Intent reply = new Intent(ACTION_SIMPLE_ACTIVITY_START_FG_SERVICE_RESULT);
        reply.setFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        Intent serviceIntent = getIntent().getParcelableExtra("service");

        try {
            final ComponentName svcName = startService(serviceIntent);
            if (svcName != null) {
                // Successful start -> the service will send the broadcast once it's
                // transitioned to the foreground
                return true;
            }
        } catch (Exception e) {
            // fall through
        }

        // Failure of some sort: the service isn't going to run as expected,
        // so report the failure here before exiting.
        reply.putExtra("result", Activity.RESULT_CANCELED);
        sendBroadcast(reply);
        return false;
    }
}
