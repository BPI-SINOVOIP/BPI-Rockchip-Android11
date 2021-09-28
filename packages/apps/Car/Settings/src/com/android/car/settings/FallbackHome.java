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
 * limitations under the License.
 */

package com.android.car.settings;

import static android.car.settings.CarSettings.Global.ENABLE_USER_SWITCH_DEVELOPER_MESSAGE;

import android.app.Activity;
import android.car.userlib.UserHelper;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager.LayoutParams;
import android.view.animation.AnimationUtils;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.android.car.settings.common.Logger;

import java.util.Objects;

/**
 * Copied over from phone settings. This covers the fallback case where no launcher is available.
 */
public class FallbackHome extends Activity {
    private static final Logger LOG = new Logger(FallbackHome.class);
    private static final int PROGRESS_TIMEOUT = 2000;

    private boolean mProvisioned;

    private boolean mFinished;

    private final Runnable mProgressTimeoutRunnable = () -> {
        View v = getLayoutInflater().inflate(
                R.layout.fallback_home_finishing_boot, /* root= */ null);
        setContentView(v);
        v.setAlpha(0f);
        v.animate()
                .alpha(1f)
                .setDuration(500)
                .setInterpolator(AnimationUtils.loadInterpolator(
                        this, android.R.interpolator.fast_out_slow_in))
                .start();
        getWindow().addFlags(LayoutParams.FLAG_KEEP_SCREEN_ON);
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Set ourselves totally black before the device is provisioned
        mProvisioned = Settings.Global.getInt(getContentResolver(),
                Settings.Global.DEVICE_PROVISIONED, 0) != 0;
        int flags;
        boolean showInfo = false;
        if (!mProvisioned) {
            setTheme(R.style.FallbackHome_SetupWizard);
            flags = View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        } else {
            flags = View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            showInfo = "true".equals(Settings.Global.getString(getContentResolver(),
                    ENABLE_USER_SWITCH_DEVELOPER_MESSAGE));
        }

        if (showInfo) {
            // Display some info about the current user, which is useful to debug / track user
            // switching delays.
            // NOTE: we're manually creating the view (instead of inflating it from XML) to
            // minimize the performance impact.
            TextView view = new TextView(this);
            view.setText("FallbackHome for user " + getUserId() + ".\n\n"
                    + "This activity is displayed while the user is starting, \n"
                    + "and it will be replaced by the proper Home \n"
                    + "(once the user is unlocked).\n\n"
                    + "NOTE: this message is only shown on debuggable builds");
            view.setGravity(Gravity.CENTER);
            view.setLayoutParams(new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT));

            LinearLayout parent = new LinearLayout(this);
            parent.setOrientation(LinearLayout.VERTICAL);
            parent.setLayoutParams(new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT));
            parent.addView(view);

            setContentView(parent);
        }

        getWindow().getDecorView().setSystemUiVisibility(flags);

        registerReceiver(mReceiver, new IntentFilter(Intent.ACTION_USER_UNLOCKED));
        maybeFinish();
    }

    @Override
    protected void onResume() {
        super.onResume();
        LOG.d("onResume() for user " + getUserId() + ". Provisioned: " + mProvisioned);
        if (mProvisioned) {
            mHandler.postDelayed(mProgressTimeoutRunnable, PROGRESS_TIMEOUT);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        mHandler.removeCallbacks(mProgressTimeoutRunnable);
    }

    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mReceiver);
        if (!mFinished) {
            LOG.d("User " + getUserId() + " FallbackHome is finished");
            finishFallbackHome();
        }
    }

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            maybeFinish();
        }
    };

    private void maybeFinish() {
        if (getSystemService(UserManager.class).isUserUnlocked()) {
            final Intent homeIntent = new Intent(Intent.ACTION_MAIN)
                    .addCategory(Intent.CATEGORY_HOME);
            final ResolveInfo homeInfo = getPackageManager().resolveActivity(homeIntent, 0);
            if (Objects.equals(getPackageName(), homeInfo.activityInfo.packageName)) {
                if (UserManager.isSplitSystemUser()
                        && UserHandle.myUserId() == UserHandle.USER_SYSTEM) {
                    // This avoids the situation where the system user has no home activity after
                    // SUW and this activity continues to throw out warnings. See b/28870689.
                    return;
                }
                LOG.d("User " + getUserId() + " unlocked but no home; let's hope someone enables "
                        + "one soon?");
                mHandler.sendEmptyMessageDelayed(0, 500);
            } else {
                String homePackageName = homeInfo.activityInfo.packageName;
                if (UserHelper.isHeadlessSystemUser(getUserId())) {
                    // This is the transient state in HeadlessSystemMode to boot for user 10+.
                    LOG.d("User 0 unlocked, but will not launch real home: " + homePackageName);
                    return;
                }
                LOG.d("User " + getUserId() + " unlocked and real home (" + homePackageName
                        + ") found; let's go!");
                finishFallbackHome();
            }
        }
    }

    private void finishFallbackHome() {
        getSystemService(PowerManager.class).userActivity(SystemClock.uptimeMillis(), false);
        finishAndRemoveTask();
        mFinished = true;
    }

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            maybeFinish();
        }
    };
}
