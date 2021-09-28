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

package com.android.emulator.multidisplay;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class MultiDisplayServiceReceiver extends BroadcastReceiver {
    private static final String TAG = "MultiDispActionReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!isMyServiceRunning(context)) {
            Log.v(TAG, "startService");
            context.startService(new Intent(context, MultiDisplayService.class));
        }
    }

    private boolean isMyServiceRunning(Context context) {
        ActivityManager manager = (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        for (RunningServiceInfo service : manager.getRunningServices(Integer.MAX_VALUE)) {
            if (MultiDisplayService.class.getName().equals(service.service.getClassName())) {
                return true;
            }
        }
        return false;
    }
}
