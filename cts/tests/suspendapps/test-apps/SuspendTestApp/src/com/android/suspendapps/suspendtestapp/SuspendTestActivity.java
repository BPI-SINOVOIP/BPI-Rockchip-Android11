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

package com.android.suspendapps.suspendtestapp;

import static com.android.suspendapps.suspendtestapp.Constants.PACKAGE_NAME;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.GuardedBy;

public class SuspendTestActivity extends Activity {
    private static final String TAG = SuspendTestActivity.class.getSimpleName();
    private static final String ACTION_FINISH_TEST_ACTIVITY =
            PACKAGE_NAME + ".action.FINISH_TEST_ACTIVITY";

    static final Object sLock = new Object();
    @GuardedBy("sLock")
    static Intent sStartedIntent;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);
        if (ACTION_FINISH_TEST_ACTIVITY.equals(getIntent().getAction())) {
            finish();
        }
    }

    @Override
    protected void onStart() {
        Log.d(TAG, "onStart");
        super.onStart();
        synchronized (sLock) {
            sStartedIntent = getIntent();
            sLock.notifyAll();
        }
    }

    @Override
    public void onStop() {
        Log.d(TAG, "onStop");
        super.onStop();
        synchronized (sLock) {
            sStartedIntent = null;
            sLock.notifyAll();
        }
    }
}
