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
 * limitations under the License
 */

package android.server.wm.second;

import static android.server.wm.second.Components.EmbeddingActivity.ACTION_EMBEDDING_TEST_ACTIVITY_START;
import static android.server.wm.second.Components.EmbeddingActivity.EXTRA_EMBEDDING_COMPONENT_NAME;
import static android.server.wm.second.Components.EmbeddingActivity.EXTRA_EMBEDDING_TARGET_DISPLAY;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.server.wm.ActivityLauncher;
import android.util.Log;

public class EmbeddingActivity extends Activity {
    private static final String TAG = EmbeddingActivity.class.getSimpleName();

    private TestBroadcastReceiver mBroadcastReceiver = new TestBroadcastReceiver();

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        IntentFilter broadcastFilter = new IntentFilter(ACTION_EMBEDDING_TEST_ACTIVITY_START);
        registerReceiver(mBroadcastReceiver, broadcastFilter);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        unregisterReceiver(mBroadcastReceiver);
    }

    public class TestBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final Bundle extras = intent.getExtras();
            Log.i(TAG, "onReceive: extras=" + extras);

            if (extras == null) {
                return;
            }

            final ComponentName componentName =
                    extras.getParcelable(EXTRA_EMBEDDING_COMPONENT_NAME);
            final int displayId = extras.getInt(EXTRA_EMBEDDING_TARGET_DISPLAY);

            ActivityLauncher.checkActivityStartOnDisplay(EmbeddingActivity.this, displayId,
                    componentName);
        }
    }
}