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

package android.suspendapps.cts;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.util.concurrent.TimeUnit;

public class UnsuspendReceiver extends BroadcastReceiver {
    private static final String TAG = UnsuspendReceiver.class.getSimpleName();

    @Override
    public void onReceive(Context context, Intent intent) {
        switch (intent.getAction()) {
            case Intent.ACTION_PACKAGE_UNSUSPENDED_MANUALLY:
                boolean offered;
                try {
                    offered = DialogTests.sIncomingIntent.offer(intent, 10, TimeUnit.SECONDS);
                } catch (InterruptedException e) {
                    Log.w(TAG, "Interrupted while offering with wait. Retrying without wait");
                    offered = DialogTests.sIncomingIntent.offer(intent);
                }
                if (!offered) {
                    Log.e(TAG, "Unable to offer received intent to queue");
                }
                break;
            default:
                Log.w(TAG, "Unknown action " + intent.getAction());
                break;
        }
    }
}
