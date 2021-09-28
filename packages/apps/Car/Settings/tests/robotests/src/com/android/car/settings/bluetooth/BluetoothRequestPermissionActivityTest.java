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

package com.android.car.settings.bluetooth;

import static android.content.pm.PackageManager.FEATURE_BLUETOOTH;

import static com.google.common.truth.Truth.assertThat;

import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.widget.Button;

import com.android.car.settings.testutils.ShadowBluetoothAdapter;
import com.android.car.settings.testutils.ShadowBluetoothPan;
import com.android.car.settings.testutils.ShadowLocalBluetoothAdapter;
import com.android.settingslib.bluetooth.LocalBluetoothAdapter;
import com.android.settingslib.bluetooth.LocalBluetoothManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowResolveInfo;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowLocalBluetoothAdapter.class, ShadowBluetoothAdapter.class,
        ShadowBluetoothPan.class})
public class BluetoothRequestPermissionActivityTest {

    private Context mContext;
    private ActivityController<BluetoothRequestPermissionActivity> mActivityController;
    private BluetoothRequestPermissionActivity mActivity;
    private LocalBluetoothAdapter mAdapter;
    private PackageManager mPackageManager;

    @Before
    public void setUp() throws Exception {
        mContext = RuntimeEnvironment.application;
        mActivityController = ActivityController.of(new BluetoothRequestPermissionActivity());
        mActivity = mActivityController.get();
        mPackageManager = mContext.getPackageManager();

        mAdapter = LocalBluetoothManager.getInstance(mContext,
                /* onInitCallback= */ null).getBluetoothAdapter();

        // Make sure controller is available.
        getShadowPackageManager().setSystemFeature(
                FEATURE_BLUETOOTH, /* supported= */ true);
        BluetoothAdapter.getDefaultAdapter().enable();
    }

    @After
    public void tearDown() {
        ShadowBluetoothAdapter.reset();
    }

    @Test
    public void onCreate_requestDisableIntent_hasDisableRequestType() {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getRequestType()).isEqualTo(
                BluetoothRequestPermissionActivity.REQUEST_DISABLE);
    }

    @Test
    public void onCreate_requestDiscoverableIntent_hasDiscoverableRequestType() {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getRequestType()).isEqualTo(
                BluetoothRequestPermissionActivity.REQUEST_ENABLE_DISCOVERABLE);
    }

    @Test
    public void onCreate_requestDiscoverableIntent_noTimeoutSpecified_hasDefaultTimeout() {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getTimeout()).isEqualTo(
                BluetoothRequestPermissionActivity.DEFAULT_DISCOVERABLE_TIMEOUT);
    }

    @Test
    public void onCreate_requestDiscoverableIntent_timeoutSpecified_hasTimeout() {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        intent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION,
                BluetoothRequestPermissionActivity.MAX_DISCOVERABLE_TIMEOUT);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getTimeout()).isEqualTo(
                BluetoothRequestPermissionActivity.MAX_DISCOVERABLE_TIMEOUT);
    }

    @Test
    public void onCreate_requestDiscoverableIntent_bypassforSetup_startsDiscoverableScan() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);
        mAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_NONE);

        Intent intent = createSetupWizardIntent();
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mAdapter.getScanMode()).isEqualTo(
                BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    }

    @Test
    public void onCreate_requestDiscoverableIntent_bypassforSetup_turningOn_noDialog() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_TURNING_ON);
        mAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_NONE);

        Intent intent = createSetupWizardIntent();
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentDialog()).isNull();
    }

    @Test
    public void onCreate_requestDiscoverableIntent_bypassforSetup_turningOn_receiverRegistered() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_TURNING_ON);
        mAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_NONE);

        Intent intent = createSetupWizardIntent();
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentReceiver()).isNotNull();
    }

    @Test
    public void onCreate_requestDiscoverableIntent_bypassforSetup_turningOn_enableDiscovery() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_TURNING_ON);
        mAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_NONE);

        Intent intent = createSetupWizardIntent();
        mActivity.setIntent(intent);
        mActivityController.create();

        // Simulate bluetooth callback from STATE_TURNING_ON to STATE_ON
        Intent stateChangedIntent = new Intent();
        stateChangedIntent.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_ON);
        mActivity.getCurrentReceiver().onReceive(mContext, stateChangedIntent);

        assertThat(mAdapter.getScanMode()).isEqualTo(
                BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    }

    @Test
    public void onCreate_requestDiscoverableIntent_bypassforGeneric_noScanModeChange() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);
        mAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_NONE);

        Intent launchIntent = new Intent(Intent.ACTION_MAIN);
        launchIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        ResolveInfo launchInfo =
                ShadowResolveInfo.newResolveInfo("System Launcher", "system.launcher");
        launchInfo.activityInfo.applicationInfo.flags |= ApplicationInfo.FLAG_SYSTEM;

        getShadowPackageManager().addResolveInfoForIntent(launchIntent, launchInfo);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        intent.putExtra(BluetoothRequestPermissionActivity.EXTRA_BYPASS_CONFIRM_DIALOG, true);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mAdapter.getScanMode()).isEqualTo(BluetoothAdapter.SCAN_MODE_NONE);
    }

    @Test
    public void onCreate_requestEnableIntent_hasEnableRequestType() {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getRequestType()).isEqualTo(
                BluetoothRequestPermissionActivity.REQUEST_ENABLE);
    }

    @Test
    public void onCreate_bluetoothOff_requestDisableIntent_noDialog() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_OFF);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentDialog()).isNull();
    }

    @Test
    public void onCreate_bluetoothOn_requestDisableIntent_startsDialog() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentDialog()).isNotNull();
        assertThat(mActivity.getCurrentDialog().isShowing()).isTrue();
    }

    @Test
    public void onCreate_bluetoothOff_requestDiscoverableIntent_startsDialog() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_OFF);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentDialog()).isNotNull();
        assertThat(mActivity.getCurrentDialog().isShowing()).isTrue();
    }

    @Test
    public void onCreate_bluetoothOn_requestDiscoverableIntent_startsDialog() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentDialog()).isNotNull();
        assertThat(mActivity.getCurrentDialog().isShowing()).isTrue();
    }

    @Test
    public void onCreate_bluetoothOff_requestEnableIntent_startsDialog() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_OFF);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentDialog()).isNotNull();
        assertThat(mActivity.getCurrentDialog().isShowing()).isTrue();
    }

    @Test
    public void onCreate_bluetoothOn_requestEnableIntent_noDialog() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        assertThat(mActivity.getCurrentDialog()).isNull();
    }

    @Test
    public void onPositiveClick_disableDialog_disables() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);
        mAdapter.enable();

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        Button button = mActivity.getCurrentDialog().getButton(DialogInterface.BUTTON_POSITIVE);
        button.performClick();

        assertThat(mAdapter.isEnabled()).isFalse();
    }

    @Test
    public void onPositiveClick_discoverableDialog_scanModeSet() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);
        mAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_NONE);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        Button button = mActivity.getCurrentDialog().getButton(DialogInterface.BUTTON_POSITIVE);
        button.performClick();

        assertThat(mAdapter.getScanMode()).isEqualTo(
                BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    }

    @Test
    public void onPositiveClick_enableDialog_enables() {
        getShadowLocalBluetoothAdapter().setState(BluetoothAdapter.STATE_OFF);
        mAdapter.disable();

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        mActivity.setIntent(intent);
        mActivityController.create();

        Button button = mActivity.getCurrentDialog().getButton(DialogInterface.BUTTON_POSITIVE);
        button.performClick();

        assertThat(mAdapter.isEnabled()).isTrue();
    }

    private Intent createSetupWizardIntent() {
        String callerPackageName = "system.suwapp";
        Intent setupIntent = new Intent(Intent.ACTION_MAIN);
        setupIntent.addCategory(Intent.CATEGORY_SETUP_WIZARD);
        ResolveInfo suwInfo =
                ShadowResolveInfo.newResolveInfo("SetupWizard app", callerPackageName);
        Shadows.shadowOf(mActivity).setCallingPackage(callerPackageName);
        suwInfo.activityInfo.applicationInfo.flags |= ApplicationInfo.FLAG_SYSTEM;

        getShadowPackageManager().addResolveInfoForIntent(setupIntent, suwInfo);

        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
        intent.putExtra(BluetoothRequestPermissionActivity.EXTRA_BYPASS_CONFIRM_DIALOG, true);
        return intent;
    }

    private ShadowLocalBluetoothAdapter getShadowLocalBluetoothAdapter() {
        return (ShadowLocalBluetoothAdapter) Shadow.extract(mAdapter);
    }

    private ShadowPackageManager getShadowPackageManager() {
        return Shadow.extract(mPackageManager);
    }
}
