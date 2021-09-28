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

package android.server.wm.app;

import static android.server.wm.app.Components.LaunchingActivity.KEY_FINISH_BEFORE_LAUNCH;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.server.wm.ActivityLauncher;
import android.server.wm.CommandSession;

/**
 * Activity that launches another activities when new intent is received.
 */
public class LaunchingActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Intent intent = getIntent();
        if (intent != null && intent.getExtras() != null
                && intent.getExtras().getBoolean(KEY_FINISH_BEFORE_LAUNCH)) {
            finish();
        }
        if (savedInstanceState == null && intent != null) {
            launchActivityFromExtras(intent.getExtras());
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        launchActivityFromExtras(intent.getExtras());
    }

    private void launchActivityFromExtras(Bundle extras) {
        ActivityLauncher.launchActivityFromExtras(
                this, extras, CommandSession.handleForward(extras));
    }
}

