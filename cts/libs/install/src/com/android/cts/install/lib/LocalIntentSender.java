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

package com.android.cts.install.lib;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageInstaller;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Helper for making IntentSenders whose results are sent back to the test
 * app.
 */
public class LocalIntentSender extends BroadcastReceiver {
    private static final String TAG = "cts.install.lib";

    private static final BlockingQueue<Intent> sIntentSenderResults = new LinkedBlockingQueue<>();

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.i(TAG, "Received intent " + prettyPrint(intent));
        sIntentSenderResults.add(intent);
    }

    /**
     * Get a LocalIntentSender.
     */
    public static IntentSender getIntentSender() {
        Context context = InstrumentationRegistry.getContext();
        Intent intent = new Intent(context, LocalIntentSender.class);
        PendingIntent pending = PendingIntent.getBroadcast(context, 0, intent, 0);
        return pending.getIntentSender();
    }

    /**
     * Returns the most recent Intent sent by a LocalIntentSender.
     */
    public static Intent getIntentSenderResult() throws InterruptedException {
        Intent intent = sIntentSenderResults.take();
        Log.i(TAG, "Taking intent " + prettyPrint(intent));
        return intent;
    }

    /**
     * Returns an Intent that targets the given {@code sessionId}, while discarding others.
     */
    public static Intent getIntentSenderResult(int sessionId) throws InterruptedException {
        while (true) {
            Intent intent = sIntentSenderResults.take();
            if (intent.getIntExtra(PackageInstaller.EXTRA_SESSION_ID, -1) == sessionId) {
                Log.i(TAG, "Taking intent " + prettyPrint(intent));
                return intent;
            } else {
                Log.i(TAG, "Discarding intent " + prettyPrint(intent));
            }
        }
    }

    private static String prettyPrint(Intent intent) {
        int sessionId = intent.getIntExtra(PackageInstaller.EXTRA_SESSION_ID, -1);
        int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS,
                PackageInstaller.STATUS_FAILURE);
        String message = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
        return String.format("%s: {\n"
                + "sessionId = %d\n"
                + "status = %d\n"
                + "message = %s\n"
                + "}", intent, sessionId, status, message);
    }
}
