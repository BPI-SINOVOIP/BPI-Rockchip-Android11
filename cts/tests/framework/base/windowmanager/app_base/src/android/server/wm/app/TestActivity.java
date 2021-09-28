/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.server.wm.app;

import static android.server.wm.app.Components.TestActivity.COMMAND_NAVIGATE_UP_TO;
import static android.server.wm.app.Components.TestActivity.EXTRA_CONFIG_ASSETS_SEQ;
import static android.server.wm.app.Components.TestActivity.EXTRA_FIXED_ORIENTATION;
import static android.server.wm.app.Components.TestActivity.EXTRA_INTENTS;
import static android.server.wm.app.Components.TestActivity.EXTRA_INTENT;
import static android.server.wm.app.Components.TestActivity.EXTRA_NO_IDLE;
import static android.server.wm.app.Components.TestActivity.TEST_ACTIVITY_ACTION_FINISH_SELF;
import static android.server.wm.app.Components.TestActivity.COMMAND_START_ACTIVITIES;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Looper;
import android.os.Parcelable;
import android.view.animation.Animation;
import android.view.animation.RotateAnimation;
import android.widget.ProgressBar;

import java.util.Arrays;

public class TestActivity extends AbstractLifecycleLogActivity {

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent != null && TEST_ACTIVITY_ACTION_FINISH_SELF.equals(intent.getAction())) {
                finish();
            }
        }
    };

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        // Set the fixed orientation if requested
        if (getIntent().hasExtra(EXTRA_FIXED_ORIENTATION)) {
            final int ori = Integer.parseInt(getIntent().getStringExtra(EXTRA_FIXED_ORIENTATION));
            setRequestedOrientation(ori);
        }

        if (getIntent().hasExtra(EXTRA_NO_IDLE)) {
            preventAcitivtyIdle();
        }
    }

    /** Starts a repeated animation on main thread to make its message queue non-empty. */
    private void preventAcitivtyIdle() {
        final ProgressBar progressBar = new ProgressBar(this);
        progressBar.setIndeterminate(true);
        setContentView(progressBar);
        final RotateAnimation animation = new RotateAnimation(0, 180, Animation.RELATIVE_TO_SELF,
                0.5f, Animation.RELATIVE_TO_SELF, 0.5f);
        animation.setRepeatCount(Animation.INFINITE);
        progressBar.startAnimation(animation);

        Looper.myLooper().getQueue().addIdleHandler(() -> {
            if (progressBar.isAnimating()) {
                throw new RuntimeException("Shouldn't receive idle while animating");
            }
            return false;
        });
    }

    @Override
    protected void onStart() {
        super.onStart();
        registerReceiver(mReceiver, new IntentFilter(TEST_ACTIVITY_ACTION_FINISH_SELF));
    }

    @Override
    protected void onResume() {
        super.onResume();
        final Configuration configuration = getResources().getConfiguration();
        dumpConfiguration(configuration);
        dumpAssetSeqNumber(configuration);
        dumpConfigInfo();
    }

    @Override
    protected void onStop() {
        super.onStop();
        unregisterReceiver(mReceiver);
    }

    @Override
    public void handleCommand(String command, Bundle data) {
        switch (command) {
            case COMMAND_START_ACTIVITIES:
                final Parcelable[] intents = data.getParcelableArray(EXTRA_INTENTS);
                startActivities(Arrays.copyOf(intents, intents.length, Intent[].class));
                break;
            case COMMAND_NAVIGATE_UP_TO:
                navigateUpTo(data.getParcelable(EXTRA_INTENT));
                break;
            default:
                super.handleCommand(command, data);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        dumpConfiguration(newConfig);
        dumpAssetSeqNumber(newConfig);
        dumpConfigInfo();
    }

    private void dumpAssetSeqNumber(Configuration newConfig) {
        withTestJournalClient(client -> {
            final Bundle extras = new Bundle();
            extras.putInt(EXTRA_CONFIG_ASSETS_SEQ, newConfig.assetsSeq);
            client.putExtras(extras);
        });
    }
}
