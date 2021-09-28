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

package android.app.notification.legacy.cts;

import static android.app.NotificationManager.ACTION_INTERRUPTION_FILTER_CHANGED;
import static java.lang.Thread.sleep;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

public class ZenModeBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction() == null ||
            intent.getPackage() == null) {
            return;
        }

        if (intent.getAction().equals(ACTION_INTERRUPTION_FILTER_CHANGED) &&
            intent.getPackage().equals(InstrumentationRegistry.getContext().getPackageName())) {
            mCount++;
        }
    }

    public void reset() {
        mCount = 0;
        mReset = true;
    }

    public void waitFor(int count, int ms) throws IllegalStateException {
        if (!mReset) {
            throw new IllegalStateException("Call reset() before waitFor()!");
        }
        mReset = false;

        final int delayMs = 100;
        while (ms > 0 && mCount < count) {
            ms -= delayMs;
            try {
                sleep(delayMs);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        Log.d(TAG, "Exit from wait due to " +
                (mCount < count ? "timeout" : "intents") + ".");
    }

    private static String TAG = "CpsTest";
    private int mCount = 0;
    private boolean mReset = true;
}
