/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.cts.suspensionchecker;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Activity that sends a local broadcast when launched, so that the launch can be verified.
 */
public class LaunchCheckingActivity extends Activity {
    /** Broadcast action that is used to signal successful activity launch. */
    private static final String LAUNCHED_BROADCAST_ACTION =
            "com.android.cts.suspensionchecker.LAUNCHED";

    public static boolean checkLaunch(Context context) throws InterruptedException {
        final CountDownLatch activityStartedLatch = new CountDownLatch(1);

        LocalBroadcastManager.getInstance(context).registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                activityStartedLatch.countDown();
            }
        }, new IntentFilter(LAUNCHED_BROADCAST_ACTION));

        for (int i = 0; i < 3; i++) {
            final Intent intent = new Intent()
                    .setComponent(new ComponentName(context, LaunchCheckingActivity.class))
                    // Required when launching not from activity context.
                    .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);

            if (activityStartedLatch.await(1, TimeUnit.SECONDS)) {
                return true;
            }

            // Press back in case there is a transparency dialog that is blocking the launch.
            UiDevice.getInstance(InstrumentationRegistry.getInstrumentation()).pressBack();
        }

        return false;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LocalBroadcastManager.getInstance(this)
                .sendBroadcast(new Intent(LAUNCHED_BROADCAST_ACTION));

        finish();
    }
}
