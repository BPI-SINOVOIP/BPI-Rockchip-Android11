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

package com.android.car.settings.wifi;

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.TetheringManager;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiManager;

import androidx.test.core.app.ApplicationProvider;

import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowCarWifiManager;
import com.android.car.settings.testutils.ShadowConnectivityManager;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowApplication;

import java.util.concurrent.Executor;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowCarWifiManager.class, ShadowConnectivityManager.class})
public class WifiTetherFragmentTest {

    private Context mContext;
    private WifiTetherFragment mFragment;
    private FragmentController<WifiTetherFragment> mFragmentController;
    @Mock
    private CarWifiManager mCarWifiManager;
    @Mock
    private TetheringManager mTetheringManager;
    @Mock
    private WifiManager mWifiManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        getShadowApplication(mContext).setSystemService(
                Context.TETHERING_SERVICE, mTetheringManager);
        when(mWifiManager.getSoftApConfiguration()).thenReturn(
                new SoftApConfiguration.Builder().build());
        getShadowApplication(mContext).setSystemService(
                Context.WIFI_SERVICE, mWifiManager);
        mFragment = new WifiTetherFragment();
        mFragmentController = FragmentController.of(mFragment);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @After
    public void tearDown() {
        ShadowCarWifiManager.reset();
    }

    @Test
    public void onStart_tetherStateOn_shouldReturnSwitchStateOn() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(true);
        ShadowCarWifiManager.setInstance(mCarWifiManager);

        mFragmentController.setup();

        assertThat(findSwitch(mFragment.requireActivity()).isChecked()).isTrue();
    }

    @Test
    public void onStart_tetherStateOff_shouldReturnSwitchStateOff() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(false);
        ShadowCarWifiManager.setInstance(mCarWifiManager);

        mFragmentController.setup();

        assertThat(findSwitch(mFragment.requireActivity()).isChecked()).isFalse();
    }

    @Test
    public void onSwitchOn_shouldAttemptTetherOn() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(false);
        ShadowCarWifiManager.setInstance(mCarWifiManager);

        mFragmentController.setup();
        findSwitch(mFragment.requireActivity()).performClick();

        verify(mTetheringManager).startTethering(
                eq(ConnectivityManager.TETHERING_WIFI),
                any(Executor.class), any(TetheringManager.StartTetheringCallback.class)
        );
    }

    @Test
    public void onSwitchOff_shouldAttemptTetherOff() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(true);
        ShadowCarWifiManager.setInstance(mCarWifiManager);

        mFragmentController.setup();
        findSwitch(mFragment.requireActivity()).performClick();

        verify(mTetheringManager).stopTethering(ConnectivityManager.TETHERING_WIFI);
    }

    @Test
    public void onTetherEnabling_shouldReturnSwitchStateDisabled() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(false);
        ShadowCarWifiManager.setInstance(mCarWifiManager);
        mFragmentController.setup();

        Intent intent = new Intent(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);
        intent.putExtra(WifiManager.EXTRA_WIFI_AP_STATE, WifiManager.WIFI_AP_STATE_ENABLING);
        mContext.sendBroadcast(intent);
        Robolectric.flushForegroundThreadScheduler();

        assertThat(findSwitch(mFragment.requireActivity()).isEnabled()).isFalse();
    }

    @Test
    public void onTetherEnabled_shouldReturnSwitchStateEnabledAndOn() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(false);
        ShadowCarWifiManager.setInstance(mCarWifiManager);
        mFragmentController.setup();

        Intent intent = new Intent(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);
        intent.putExtra(WifiManager.EXTRA_WIFI_AP_STATE, WifiManager.WIFI_AP_STATE_ENABLED);
        mContext.sendBroadcast(intent);
        Robolectric.flushForegroundThreadScheduler();

        assertThat(findSwitch(mFragment.requireActivity()).isEnabled()).isTrue();
        assertThat(findSwitch(mFragment.requireActivity()).isChecked()).isTrue();
    }

    @Test
    public void onTetherDisabled_shouldReturnSwitchStateEnabledAndOff() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(false);
        ShadowCarWifiManager.setInstance(mCarWifiManager);
        mFragmentController.setup();

        Intent intent = new Intent(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);
        intent.putExtra(WifiManager.EXTRA_WIFI_AP_STATE, WifiManager.WIFI_AP_STATE_DISABLED);
        mContext.sendBroadcast(intent);
        Robolectric.flushForegroundThreadScheduler();

        assertThat(findSwitch(mFragment.requireActivity()).isEnabled()).isTrue();
        assertThat(findSwitch(mFragment.requireActivity()).isChecked()).isFalse();
    }

    @Test
    public void onEnableTetherFailed_shouldReturnSwitchStateEnabledAndOff() {
        when(mCarWifiManager.isWifiApEnabled()).thenReturn(false);
        ShadowCarWifiManager.setInstance(mCarWifiManager);
        mFragmentController.setup();

        Intent intent = new Intent(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);
        intent.putExtra(WifiManager.EXTRA_WIFI_AP_STATE, WifiManager.WIFI_AP_STATE_ENABLING);
        mContext.sendBroadcast(intent);
        Robolectric.flushForegroundThreadScheduler();

        Intent intent2 = new Intent(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);
        intent.putExtra(WifiManager.EXTRA_WIFI_AP_STATE, WifiManager.WIFI_AP_STATE_FAILED);
        mContext.sendBroadcast(intent2);

        assertThat(findSwitch(mFragment.requireActivity()).isEnabled()).isTrue();
        assertThat(findSwitch(mFragment.requireActivity()).isChecked()).isFalse();
    }

    private MenuItem findSwitch(Activity activity) {
        ToolbarController toolbar = requireToolbar(activity);
        return toolbar.getMenuItems().get(0);
    }

    private ShadowApplication getShadowApplication(Context context) {
        return Shadow.extract(context);
    }
}
