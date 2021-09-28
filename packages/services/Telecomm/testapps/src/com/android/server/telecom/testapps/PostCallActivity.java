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

package com.android.server.telecom.testapps;

import static android.telecom.TelecomManager.EXTRA_CALL_DURATION;
import static android.telecom.TelecomManager.EXTRA_DISCONNECT_CAUSE;
import static android.telecom.TelecomManager.EXTRA_HANDLE;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.telecom.Log;

public class PostCallActivity extends Activity {

    public static final String ACTION_POST_CALL = "android.telecom.action.POST_CALL";
    public static final int DEFAULT_DISCONNECT_CAUSE = -1;
    public static final int DEFAULT_DURATION = -1;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        final Intent intent = getIntent();
        final String action = intent != null ? intent.getAction() : null;
        Log.i(this, "action: %s", action);
        if (ACTION_POST_CALL.equals(action)) {
            Log.i(this, "extra handle: " +
                    intent.getParcelableExtra(EXTRA_HANDLE));
            Log.i(this, "extra disconnect cause: " +
                    intent.getIntExtra(EXTRA_DISCONNECT_CAUSE, DEFAULT_DISCONNECT_CAUSE));
            Log.i(this, "extra duration: " +
                    intent.getIntExtra(EXTRA_CALL_DURATION, DEFAULT_DURATION));
        }
    }
}
