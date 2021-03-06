/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.harmfulappwarning.sampleapp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * A simple activity which logs to Logcat.
 *
 * <p>This serves as a simple target application/activity to set a harmful app warning on.
 */
public class SampleDeviceActivity extends Activity {

    private static final String ACTION_ACTIVITY_STARTED =
            "android.harmfulappwarning.sampleapp.ACTIVITY_STARTED";

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.sample_layout);
    }

    @Override
    protected void onResume() {
        super.onResume();
        Intent intent = new Intent(ACTION_ACTIVITY_STARTED);
        sendBroadcast(intent);
    }
}
