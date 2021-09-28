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
 *
 */

package com.android.cts.verifier.forcestop;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import com.android.cts.forcestophelper.Constants;
import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

/**
 * Tests that an app is not killed when it is swiped away from recents.
 * Requires CtsForceStopHelper.apk to be installed.
 */
public class RecentTaskRemovalTestActivity extends PassFailButtons.Activity implements
        View.OnClickListener {
    private static final String HELPER_APP_NAME = Constants.PACKAGE_NAME;
    private static final String HELPER_ACTIVITY_NAME = Constants.ACTIVITY_CLASS_NAME;

    private static final String HELPER_APP_INSTALLED_KEY = "helper_installed";

    private static final String ACTION_REPORT_TASK_REMOVED = "report_task_removed";
    private static final String ACTION_REPORT_ALARM = "report_alarm";

    private static final long EXTRA_WAIT_FOR_ALARM = 2_000;

    private ImageView mInstallStatus;
    private TextView mInstallTestAppText;

    private ImageView mLaunchStatus;
    private Button mLaunchTestAppButton;

    private ImageView mRemoveFromRecentsStatus;
    private TextView mRemoveFromRecentsInstructions;

    private ImageView mForceStopStatus;
    private TextView mForceStopVerificationResult;

    private volatile boolean mTestAppInstalled;
    private volatile boolean mTestTaskLaunched;
    private volatile boolean mTestTaskRemoved;
    private volatile boolean mTestAppForceStopped;
    private volatile boolean mTestAlarmReceived;
    private volatile boolean mWaitingForAlarm;

    private final PackageStateReceiver mPackageChangesListener = new PackageStateReceiver();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.force_stop_recents_main);
        setInfoResources(R.string.remove_from_recents_test, R.string.remove_from_recents_test_info,
                -1);
        setPassFailButtonClickListeners();

        if (savedInstanceState != null) {
            mTestAppInstalled = savedInstanceState.getBoolean(HELPER_APP_INSTALLED_KEY, false);
        } else {
            mTestAppInstalled = isPackageInstalled();
        }
        mInstallStatus = findViewById(R.id.fs_test_app_install_status);
        mInstallTestAppText = findViewById(R.id.fs_test_app_install_instructions);

        mRemoveFromRecentsStatus = findViewById(R.id.fs_test_app_recents_status);
        mRemoveFromRecentsInstructions = findViewById(R.id.fs_test_app_recents_instructions);

        mLaunchStatus = findViewById(R.id.fs_test_app_launch_status);
        mLaunchTestAppButton = findViewById(R.id.fs_launch_test_app_button);
        mLaunchTestAppButton.setOnClickListener(this);

        mForceStopStatus = findViewById(R.id.fs_force_stop_status);
        mForceStopVerificationResult = findViewById(R.id.fs_force_stop_verification);

        mPackageChangesListener.register(mForceStopStatus.getHandler());
    }

    private boolean isPackageInstalled() {
        PackageInfo packageInfo = null;
        try {
            packageInfo = getPackageManager().getPackageInfo(HELPER_APP_NAME, 0);
        } catch (PackageManager.NameNotFoundException exc) {
            // fall through
        }
        return packageInfo != null;
    }

    @Override
    public void onClick(View v) {
        if (v == mLaunchTestAppButton) {
            mTestTaskLaunched = true;

            final Intent reportTaskRemovedIntent = new Intent(ACTION_REPORT_TASK_REMOVED)
                    .setPackage(getPackageName())
                    .addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
            final PendingIntent onTaskRemoved = PendingIntent.getBroadcast(this, 0,
                    reportTaskRemovedIntent, 0);

            final Intent reportAlarmIntent = new Intent(ACTION_REPORT_ALARM)
                    .setPackage(getPackageName())
                    .addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
            final PendingIntent onAlarm = PendingIntent.getBroadcast(this, 0, reportAlarmIntent, 0);

            final Intent testActivity = new Intent()
                    .setClassName(HELPER_APP_NAME, HELPER_ACTIVITY_NAME)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra(Constants.EXTRA_ON_TASK_REMOVED, onTaskRemoved)
                    .putExtra(Constants.EXTRA_ON_ALARM, onAlarm);
            startActivity(testActivity);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        updateWidgets();
    }

    @Override
    public void onSaveInstanceState(Bundle icicle) {
        icicle.putBoolean(HELPER_APP_INSTALLED_KEY, mTestAppInstalled);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mPackageChangesListener.unregister();
    }

    private void updateWidgets() {
        mInstallStatus.setImageResource(
                mTestAppInstalled ? R.drawable.fs_good : R.drawable.fs_indeterminate);
        mInstallTestAppText.setText(mTestAppInstalled ? R.string.fs_test_app_installed_text
                : R.string.fs_test_app_install_instructions);
        mInstallStatus.invalidate();

        mLaunchStatus.setImageResource(
                mTestTaskLaunched ? R.drawable.fs_good : R.drawable.fs_indeterminate);
        mLaunchTestAppButton.setEnabled(mTestAppInstalled && !mTestTaskLaunched);
        mLaunchStatus.invalidate();

        mRemoveFromRecentsStatus.setImageResource(
                mTestTaskRemoved ? R.drawable.fs_good : R.drawable.fs_indeterminate);
        mRemoveFromRecentsInstructions.setText(R.string.fs_test_app_recents_instructions);
        mRemoveFromRecentsStatus.invalidate();

        if (mTestTaskRemoved) {
            if (mWaitingForAlarm) {
                mForceStopStatus.setImageResource(R.drawable.fs_clock);
                mForceStopVerificationResult.setText(R.string.fs_force_stop_verification_pending);
            } else {
                mForceStopStatus.setImageResource(
                        (mTestAppForceStopped || !mTestAlarmReceived) ? R.drawable.fs_error
                                : R.drawable.fs_good);
                mForceStopVerificationResult.setText((mTestAppForceStopped || !mTestAlarmReceived)
                        ? R.string.result_failure
                        : R.string.result_success);
            }
            mForceStopStatus.invalidate();
            mForceStopStatus.setVisibility(View.VISIBLE);
            mForceStopVerificationResult.setVisibility(View.VISIBLE);
        } else {
            mForceStopStatus.setVisibility(View.GONE);
            mForceStopVerificationResult.setVisibility(View.GONE);
        }

        getPassButton().setEnabled(mTestAlarmReceived && !mTestAppForceStopped);
    }

    private final class PackageStateReceiver extends BroadcastReceiver {

        void register(Handler handler) {
            final IntentFilter packageFilter = new IntentFilter();
            packageFilter.addAction(Intent.ACTION_PACKAGE_RESTARTED);
            packageFilter.addAction(Intent.ACTION_PACKAGE_ADDED);
            packageFilter.addAction(Intent.ACTION_PACKAGE_REMOVED);
            packageFilter.addDataScheme("package");
            registerReceiver(this, packageFilter);

            final IntentFilter commsFilter = new IntentFilter();
            commsFilter.addAction(ACTION_REPORT_TASK_REMOVED);
            commsFilter.addAction(ACTION_REPORT_ALARM);
            registerReceiver(this, commsFilter, null, handler);
        }

        void unregister() {
            unregisterReceiver(this);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            final Uri uri = intent.getData();
            boolean testPackageAffected = (uri != null && HELPER_APP_NAME.equals(
                    uri.getSchemeSpecificPart()));
            switch (intent.getAction()) {
                case Intent.ACTION_PACKAGE_ADDED:
                case Intent.ACTION_PACKAGE_REMOVED:
                    if (testPackageAffected) {
                        mTestAppInstalled = Intent.ACTION_PACKAGE_ADDED.equals(intent.getAction());
                    }
                    break;
                case Intent.ACTION_PACKAGE_RESTARTED:
                    if (testPackageAffected) {
                        mTestAppForceStopped = true;
                    }
                    break;
                case ACTION_REPORT_TASK_REMOVED:
                    mTestTaskRemoved = true;
                    mWaitingForAlarm = true;
                    mForceStopStatus.postDelayed(() -> {
                        mWaitingForAlarm = false;
                        updateWidgets();
                    }, Constants.ALARM_DELAY + EXTRA_WAIT_FOR_ALARM);
                    break;
                case ACTION_REPORT_ALARM:
                    mTestAlarmReceived = true;
                    mWaitingForAlarm = false;
                    break;
            }
            updateWidgets();
        }
    }
}
