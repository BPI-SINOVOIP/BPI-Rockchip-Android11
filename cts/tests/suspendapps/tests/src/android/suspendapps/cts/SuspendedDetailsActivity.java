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

import android.app.Activity;
import android.content.Intent;
import android.util.Log;

import java.util.concurrent.TimeUnit;

public class SuspendedDetailsActivity extends Activity {
    private static final String TAG = SuspendedDetailsActivity.class.getSimpleName();

    @Override
    protected void onStart() {
        super.onStart();
        final Intent startedIntent = getIntent();
        boolean offered;
        try {
            offered = DialogTests.sIncomingIntent.offer(startedIntent, 10, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.w(TAG, "Interrupted while offering intent to queue. Retrying without wait");
            offered = DialogTests.sIncomingIntent.offer(startedIntent);
        }
        if (!offered) {
            Log.e(TAG, "Failed to offer started intent " + startedIntent + " to queue");
        }
        finish();
    }
}
