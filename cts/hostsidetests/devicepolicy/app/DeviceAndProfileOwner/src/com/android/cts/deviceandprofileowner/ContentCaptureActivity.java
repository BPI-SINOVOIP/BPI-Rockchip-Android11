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

package com.android.cts.deviceandprofileowner;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Wrapper class used to call the activity in the non-test APK and wait for its result.
 */
public class ContentCaptureActivity extends Activity {
    private static final String TAG = "ContentCaptureActivity";

    public static final String CONTENT_CAPTURE_PACKAGE_NAME =
            "com.android.cts.devicepolicy.contentcaptureapp";
    public static final String CONTENT_CAPTURE_ACTIVITY_NAME = CONTENT_CAPTURE_PACKAGE_NAME
            + ".SimpleActivity";

    private CountDownLatch mEnabledLatch = new CountDownLatch(1);
    private CountDownLatch mDisabledLatch = new CountDownLatch(1);

    private ContentCaptureActivityReceiver mReceiver;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mReceiver = new ContentCaptureActivityReceiver();
        registerReceiver(mReceiver, new IntentFilter(ContentCaptureActivityReceiver.ACTION));

        final Intent launchIntent = new Intent();
        launchIntent.setComponent(
                new ComponentName(CONTENT_CAPTURE_PACKAGE_NAME, CONTENT_CAPTURE_ACTIVITY_NAME));
        startActivityForResult(launchIntent, 42);
    }

    public void waitContentCaptureEnabled(boolean enabled) throws InterruptedException {
        final Intent broadcastIntent = new Intent().setAction("SimpleActivityReceiver");
        sendBroadcast(broadcastIntent);

        final CountDownLatch latch = enabled ? mEnabledLatch : mDisabledLatch;
        final boolean called = latch.await(2, TimeUnit.SECONDS);

        if (!called) {
            Log.e(TAG, CONTENT_CAPTURE_PACKAGE_NAME
                    + " didn't get " + (enabled ? "enabled" : "disabled") + " state in 2 seconds");
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mReceiver);
    }

    private class ContentCaptureActivityReceiver extends BroadcastReceiver {
        private static final String ACTION = "ContentCaptureActivityReceiver";

        @Override
        public void onReceive(Context context, Intent intent) {
            boolean enabled = intent.getBooleanExtra("enabled", false);
            if (enabled) {
                if (mEnabledLatch.getCount() == 0) {
                    Log.e(TAG, "already received enabled broadcast");
                }
                mEnabledLatch.countDown();
            } else {
                if (mDisabledLatch.getCount() == 0) {
                    Log.e(TAG, "already received disabled broadcast");
                }
                mDisabledLatch.countDown();
            }
        }
    }
}
