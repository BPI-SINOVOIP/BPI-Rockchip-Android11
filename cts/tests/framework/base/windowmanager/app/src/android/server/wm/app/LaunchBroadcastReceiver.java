/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * limitations under the License
 */

package android.server.wm.app;

import static android.server.wm.app.Components.LaunchBroadcastReceiver.ACTION_TEST_ACTIVITY_START;
import static android.server.wm.app.Components.LaunchBroadcastReceiver.EXTRA_COMPONENT_NAME;
import static android.server.wm.app.Components.LaunchBroadcastReceiver.EXTRA_TARGET_DISPLAY;
import static android.server.wm.app.Components.LaunchBroadcastReceiver.LAUNCH_BROADCAST_ACTION;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.server.wm.ActivityLauncher;
import android.server.wm.CommandSession;
import android.util.Log;

/** Broadcast receiver that can launch activities. */
public class LaunchBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = LaunchBroadcastReceiver.class.getSimpleName();

    @Override
    public void onReceive(Context context, Intent intent) {
        final Bundle extras = intent.getExtras();
        Log.i(TAG, "onReceive: extras=" + extras);
        if (extras == null) {
            Log.e(TAG, "No extras received");
            return;
        }

        if (intent.getAction().equals(LAUNCH_BROADCAST_ACTION)) {
            try {
                ActivityLauncher.launchActivityFromExtras(context, extras,
                        CommandSession.handleForward(extras));
            } catch (SecurityException e) {
                ActivityLauncher.handleSecurityException(context, e);
            }
        } else if (intent.getAction().equals(ACTION_TEST_ACTIVITY_START)) {
            final ComponentName componentName = extras.getParcelable(EXTRA_COMPONENT_NAME);
            final int displayId = extras.getInt(EXTRA_TARGET_DISPLAY);
            ActivityLauncher.checkActivityStartOnDisplay(context, displayId, componentName);
        }
    }
}
