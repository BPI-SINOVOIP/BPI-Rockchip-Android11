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

package com.android.cts.dummylauncher;

import android.app.Activity;
import android.os.Bundle;
import android.os.Process;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import java.util.List;

public class QuietModeToggleActivity extends Activity {
    private static final String TAG = "QuietModeToggleActivity";
    private static final String EXTRA_QUIET_MODE_STATE =
            "com.android.cts.dummyactivity.QUIET_MODE_STATE";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        toggleQuietMode();
        finish();
    }

    private void toggleQuietMode() {
        final boolean quietModeState = getIntent().getBooleanExtra(EXTRA_QUIET_MODE_STATE, false);
        final UserManager userManager = UserManager.get(this);

        final List<UserHandle> users = userManager.getUserProfiles();
        if (users.size() != 2) {
            Log.e(TAG, "Unexpected number of profiles: " + users.size());
            return;
        }

        final UserHandle profileHandle =
                users.get(0).equals(Process.myUserHandle()) ? users.get(1) : users.get(0);

        final String quietModeStateString = quietModeState ? "enabled" : "disabled";
        if (userManager.isQuietModeEnabled(profileHandle) == quietModeState) {
            Log.w(TAG, "Quiet mode is already " + quietModeStateString);
            return;
        }

        userManager.requestQuietModeEnabled(quietModeState, profileHandle);
        Log.i(TAG, "Quiet mode for user " + profileHandle + " was set to " + quietModeStateString);
    }
}