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
 * limitations under the License
 */

package android.server.wm.app;

import static android.app.WindowConfiguration.WINDOWING_MODE_PINNED;
import static android.content.Intent.FLAG_ACTIVITY_CLEAR_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.app.Components.PipActivity.ACTION_ENTER_PIP;
import static android.server.wm.app.Components.PipActivity.ACTION_EXPAND_PIP;
import static android.server.wm.app.Components.PipActivity.ACTION_FINISH;
import static android.server.wm.app.Components.PipActivity.ACTION_MOVE_TO_BACK;
import static android.server.wm.app.Components.PipActivity.ACTION_ON_PIP_REQUESTED;
import static android.server.wm.app.Components.PipActivity.ACTION_SET_REQUESTED_ORIENTATION;
import static android.server.wm.app.Components.PipActivity.EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP;
import static android.server.wm.app.Components.PipActivity.EXTRA_DISMISS_KEYGUARD;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ON_PAUSE;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ON_PIP_REQUESTED;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ON_USER_LEAVE_HINT;
import static android.server.wm.app.Components.PipActivity.EXTRA_FINISH_SELF_ON_RESUME;
import static android.server.wm.app.Components.PipActivity.EXTRA_ON_PAUSE_DELAY;
import static android.server.wm.app.Components.PipActivity.EXTRA_PIP_ORIENTATION;
import static android.server.wm.app.Components.PipActivity.EXTRA_SET_ASPECT_RATIO_DENOMINATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_SET_ASPECT_RATIO_NUMERATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_SET_ASPECT_RATIO_WITH_DELAY_DENOMINATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_SET_ASPECT_RATIO_WITH_DELAY_NUMERATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_SHOW_OVER_KEYGUARD;
import static android.server.wm.app.Components.PipActivity.EXTRA_START_ACTIVITY;
import static android.server.wm.app.Components.PipActivity.EXTRA_TAP_TO_FINISH;
import static android.view.WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PictureInPictureParams;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.server.wm.CommandSession;
import android.util.Log;
import android.util.Rational;

public class PipActivity extends AbstractLifecycleLogActivity {

    private boolean mEnteredPictureInPicture;

    private Handler mHandler = new Handler();
    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent != null) {
                switch (intent.getAction()) {
                    case ACTION_ENTER_PIP:
                        enterPictureInPictureMode();
                        break;
                    case ACTION_MOVE_TO_BACK:
                        moveTaskToBack(false /* nonRoot */);
                        break;
                    case ACTION_EXPAND_PIP:
                        // Trigger the activity to expand
                        Intent startIntent = new Intent(PipActivity.this, PipActivity.class);
                        startIntent.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
                        startActivity(startIntent);

                        if (intent.hasExtra(EXTRA_SET_ASPECT_RATIO_WITH_DELAY_NUMERATOR)
                                && intent.hasExtra(EXTRA_SET_ASPECT_RATIO_WITH_DELAY_DENOMINATOR)) {
                            // Ugly, but required to wait for the startActivity to actually start
                            // the activity...
                            mHandler.postDelayed(() -> {
                                final PictureInPictureParams.Builder builder =
                                        new PictureInPictureParams.Builder();
                                builder.setAspectRatio(getAspectRatio(intent,
                                        EXTRA_SET_ASPECT_RATIO_WITH_DELAY_NUMERATOR,
                                        EXTRA_SET_ASPECT_RATIO_WITH_DELAY_DENOMINATOR));
                                setPictureInPictureParams(builder.build());
                            }, 100);
                        }
                        break;
                    case ACTION_SET_REQUESTED_ORIENTATION:
                        setRequestedOrientation(Integer.parseInt(intent.getStringExtra(
                                EXTRA_PIP_ORIENTATION)));
                        break;
                    case ACTION_FINISH:
                        finish();
                        break;
                    case ACTION_ON_PIP_REQUESTED:
                        onPictureInPictureRequested();
                        break;
                }
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Set the fixed orientation if requested
        if (getIntent().hasExtra(EXTRA_PIP_ORIENTATION)) {
            final int ori = Integer.parseInt(getIntent().getStringExtra(EXTRA_PIP_ORIENTATION));
            setRequestedOrientation(ori);
        }

        // Set the window flag to show over the keyguard
        if (getIntent().hasExtra(EXTRA_SHOW_OVER_KEYGUARD)) {
            setShowWhenLocked(true);
        }

        // Set the window flag to dismiss the keyguard
        if (getIntent().hasExtra(EXTRA_DISMISS_KEYGUARD)) {
            getWindow().addFlags(FLAG_DISMISS_KEYGUARD);
        }

        boolean enteringPip = false;
        // Enter picture in picture with the given aspect ratio if provided
        if (getIntent().hasExtra(EXTRA_ENTER_PIP)) {
            if (getIntent().hasExtra(EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR)
                    && getIntent().hasExtra(EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR)) {
                try {
                    final PictureInPictureParams.Builder builder =
                            new PictureInPictureParams.Builder();
                    builder.setAspectRatio(getAspectRatio(getIntent(),
                            EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR,
                            EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR));
                    enteringPip = enterPictureInPictureMode(builder.build());
                } catch (Exception e) {
                    // This call can fail intentionally if the aspect ratio is too extreme
                }
            } else {
                enteringPip = enterPictureInPictureMode(
                        new PictureInPictureParams.Builder().build());
            }
        }

        // We need to wait for either enterPictureInPicture() or requestAutoEnterPictureInPicture()
        // to be called before setting the aspect ratio
        if (getIntent().hasExtra(EXTRA_SET_ASPECT_RATIO_NUMERATOR)
                && getIntent().hasExtra(EXTRA_SET_ASPECT_RATIO_DENOMINATOR)) {
            final PictureInPictureParams.Builder builder =
                    new PictureInPictureParams.Builder();
            builder.setAspectRatio(getAspectRatio(getIntent(),
                    EXTRA_SET_ASPECT_RATIO_NUMERATOR, EXTRA_SET_ASPECT_RATIO_DENOMINATOR));
            try {
                setPictureInPictureParams(builder.build());
            } catch (Exception e) {
                // This call can fail intentionally if the aspect ratio is too extreme
            }
        }

        // Enable tap to finish if necessary
        if (getIntent().hasExtra(EXTRA_TAP_TO_FINISH)) {
            setContentView(R.layout.tap_to_finish_pip_layout);
            findViewById(R.id.content).setOnClickListener(v -> {
                finish();
            });
        }

        // Launch a new activity if requested
        String launchActivityComponent = getIntent().getStringExtra(EXTRA_START_ACTIVITY);
        if (launchActivityComponent != null) {
            Intent launchIntent = new Intent();
            launchIntent.setComponent(ComponentName.unflattenFromString(launchActivityComponent));
            startActivity(launchIntent);
        }

        // Register the broadcast receiver
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_ENTER_PIP);
        filter.addAction(ACTION_MOVE_TO_BACK);
        filter.addAction(ACTION_EXPAND_PIP);
        filter.addAction(ACTION_SET_REQUESTED_ORIENTATION);
        filter.addAction(ACTION_FINISH);
        filter.addAction(ACTION_ON_PIP_REQUESTED);
        registerReceiver(mReceiver, filter);

        // Don't dump configuration when entering PIP to avoid the verifier getting the intermediate
        // state. In this case it is expected that the verifier will check the changed configuration
        // after onConfigurationChanged.
        if (!enteringPip) {
            // Dump applied display metrics.
            dumpConfiguration(getResources().getConfiguration());
            dumpConfigInfo();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Finish self if requested
        if (getIntent().hasExtra(EXTRA_FINISH_SELF_ON_RESUME)) {
            finish();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();

        // Pause if requested
        if (getIntent().hasExtra(EXTRA_ON_PAUSE_DELAY)) {
            SystemClock.sleep(Long.valueOf(getIntent().getStringExtra(EXTRA_ON_PAUSE_DELAY)));
        }

        // Enter PIP on move to background
        if (getIntent().hasExtra(EXTRA_ENTER_PIP_ON_PAUSE)) {
            enterPictureInPictureMode(new PictureInPictureParams.Builder().build());
        }
    }

    @Override
    protected void onStop() {
        super.onStop();

        if (getIntent().hasExtra(EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP) && !mEnteredPictureInPicture) {
            Log.w(getTag(), "Unexpected onStop() called before entering picture-in-picture");
            finish();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        unregisterReceiver(mReceiver);
    }

    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();
        if (getIntent().hasExtra(EXTRA_ENTER_PIP_ON_USER_LEAVE_HINT)) {
            enterPictureInPictureMode(new PictureInPictureParams.Builder().build());
        }
    }

    @Override
    public boolean onPictureInPictureRequested() {
        onCallback(CommandSession.ActivityCallback.ON_PICTURE_IN_PICTURE_REQUESTED);
        if (getIntent().hasExtra(EXTRA_ENTER_PIP_ON_PIP_REQUESTED)) {
            enterPictureInPictureMode(new PictureInPictureParams.Builder().build());
            return true;
        }
        return super.onPictureInPictureRequested();
    }

    @Override
    public void onPictureInPictureModeChanged(boolean isInPictureInPictureMode,
            Configuration newConfig) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);

        // Fail early if the activity state does not match the dispatched state
        if (isInPictureInPictureMode() != isInPictureInPictureMode) {
            Log.w(getTag(), "Received onPictureInPictureModeChanged mode="
                    + isInPictureInPictureMode + " activityState=" + isInPictureInPictureMode());
            finish();
        }

        // Mark that we've entered picture-in-picture so that we can stop checking for
        // EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP
        if (isInPictureInPictureMode) {
            mEnteredPictureInPicture = true;
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        dumpConfiguration(newConfig);
        dumpConfigInfo();
    }

    /**
     * Launches a new instance of the PipActivity directly into the pinned stack.
     */
    static void launchActivityIntoPinnedStack(Activity caller, Rect bounds) {
        final Intent intent = new Intent(caller, PipActivity.class);
        intent.setFlags(FLAG_ACTIVITY_CLEAR_TASK | FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP, "true");

        final ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchBounds(bounds);
        options.setLaunchWindowingMode(WINDOWING_MODE_PINNED);
        caller.startActivity(intent, options.toBundle());
    }

    /**
     * Launches a new instance of the PipActivity in the same task that will automatically enter
     * PiP.
     */
    static void launchEnterPipActivity(Activity caller) {
        final Intent intent = new Intent(caller, PipActivity.class);
        intent.putExtra(EXTRA_ENTER_PIP, "true");
        intent.putExtra(EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP, "true");
        caller.startActivity(intent);
    }

    /**
     * @return a {@link Rational} aspect ratio from the given intent and extras.
     */
    private Rational getAspectRatio(Intent intent, String extraNum, String extraDenom) {
        return new Rational(
                Integer.valueOf(intent.getStringExtra(extraNum)),
                Integer.valueOf(intent.getStringExtra(extraDenom)));
    }
}
