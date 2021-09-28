/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.app.cts.activitymanager.api29;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * A simple activity to install for various users to test LauncherApps.
 */
public class SimpleActivity extends Activity {
    public static String ACTION_SIMPLE_ACTIVITY_START_RESULT =
            "android.app.cts.activitymanager.api29.SimpleActivity.RESULT";

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
    }

    @Override
    public void onStart() {
        super.onStart();
    }

    @Override
    public void onResume() {
        super.onResume();
        Intent reply = new Intent(ACTION_SIMPLE_ACTIVITY_START_RESULT);
        reply.setFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        sendBroadcast(reply);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (intent.getExtras().getBoolean("finish")) {
            finish();
        }
    }
}
