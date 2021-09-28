/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static android.server.wm.app.Components.BROADCAST_RECEIVER_ACTIVITY;
import static android.server.wm.app.Components.BroadcastReceiverActivity.ACTION_TRIGGER_BROADCAST;
import static android.server.wm.app.Components.BroadcastReceiverActivity.EXTRA_BROADCAST_ORIENTATION;
import static android.server.wm.app.Components.BroadcastReceiverActivity.EXTRA_CUTOUT_EXISTS;
import static android.server.wm.app.Components.BroadcastReceiverActivity.EXTRA_DISMISS_KEYGUARD;
import static android.server.wm.app.Components.BroadcastReceiverActivity.EXTRA_DISMISS_KEYGUARD_METHOD;
import static android.server.wm.app.Components.BroadcastReceiverActivity.EXTRA_FINISH_BROADCAST;
import static android.server.wm.app.Components.BroadcastReceiverActivity.EXTRA_MOVE_BROADCAST_TO_BACK;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.server.wm.ActivityLauncher;
import android.server.wm.TestJournalProvider;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;

import java.lang.ref.WeakReference;

/**
 * Activity that registers broadcast receiver .
 */
public class BroadcastReceiverActivity extends Activity {
    private static final String TAG = BroadcastReceiverActivity.class.getSimpleName();

    private TestBroadcastReceiver mBroadcastReceiver;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        final Object receiver = getLastNonConfigurationInstance();
        if (receiver instanceof TestBroadcastReceiver) {
            mBroadcastReceiver = (TestBroadcastReceiver) receiver;
            mBroadcastReceiver.associate(this);
        } else {
            mBroadcastReceiver = new TestBroadcastReceiver(this);
            mBroadcastReceiver.register();
        }

        // Determine if a display cutout is present
        final View view = new View(this);
        getWindow().requestFeature(Window.FEATURE_NO_TITLE);
        getWindow().getAttributes().layoutInDisplayCutoutMode =
                LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        view.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        view.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        setContentView(view);

        WindowInsets insets = getWindowManager().getCurrentWindowMetrics().getWindowInsets();
        final boolean cutoutExists = (insets.getDisplayCutout() != null);
        Log.i(TAG, "cutout=" + cutoutExists);
        TestJournalProvider.putExtras(BroadcastReceiverActivity.this,
                bundle -> bundle.putBoolean(EXTRA_CUTOUT_EXISTS, cutoutExists));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mBroadcastReceiver.destroy();
    }

    @Override
    public Object onRetainNonConfigurationInstance() {
        return mBroadcastReceiver;
    }

    /**
     * The receiver to perform action on the associated activity. If a broadcast intent is received
     * while the activity is relaunching, it will be handled after the activity is recreated.
     */
    private static class TestBroadcastReceiver extends BroadcastReceiver {
        final Context mAppContext;
        WeakReference<Activity> mRef;

        TestBroadcastReceiver(Activity activity) {
            mAppContext = activity.getApplicationContext();
            associate(activity);
        }

        void register() {
            mAppContext.registerReceiver(this, new IntentFilter(ACTION_TRIGGER_BROADCAST));
        }

        void associate(Activity activity) {
            mRef = new WeakReference<>(activity);
        }

        void destroy() {
            final Activity activity = mRef != null ? mRef.get() : null;
            if (activity != null && activity.isChangingConfigurations()) {
                // The activity is destroyed for configuration change. Because it will be recreated
                // immediately the receiver only needs to associate to the new instance.
                return;
            }
            // The case of real finish.
            mAppContext.unregisterReceiver(this);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            final Bundle extras = intent.getExtras();
            if (extras == null) {
                return;
            }
            // Trigger unparcel so the log can show the content of extras.
            extras.size();
            Log.i(TAG, "onReceive: extras=" + extras);

            final Activity activity = mRef.get();
            if (activity == null) {
                return;
            }

            if (extras.getBoolean(EXTRA_FINISH_BROADCAST)) {
                activity.finish();
            }
            if (extras.getBoolean(EXTRA_MOVE_BROADCAST_TO_BACK)) {
                activity.moveTaskToBack(true);
            }
            if (extras.containsKey(EXTRA_BROADCAST_ORIENTATION)) {
                activity.setRequestedOrientation(extras.getInt(EXTRA_BROADCAST_ORIENTATION));
            }
            if (extras.getBoolean(EXTRA_DISMISS_KEYGUARD)) {
                activity.getWindow().addFlags(FLAG_DISMISS_KEYGUARD);
            }
            if (extras.getBoolean(EXTRA_DISMISS_KEYGUARD_METHOD)) {
                activity.getSystemService(KeyguardManager.class).requestDismissKeyguard(activity,
                        new KeyguardDismissLoggerCallback(context, BROADCAST_RECEIVER_ACTIVITY));
            }

            ActivityLauncher.launchActivityFromExtras(activity, extras);
        }
    }
}
