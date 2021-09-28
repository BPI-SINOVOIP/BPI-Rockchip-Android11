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

package com.android.car.settings.wifi;

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.wifi.WifiManager;

import androidx.test.core.app.ApplicationProvider;

import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowCarWifiManager;
import com.android.car.settings.testutils.ShadowWifiManager;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

/** Unit test for {@link WifiSettingsFragment}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowCarWifiManager.class})
public class WifiSettingsFragmentTest {

    private Context mContext;
    private WifiSettingsFragment mFragment;
    private FragmentController<WifiSettingsFragment> mFragmentController;
    @Mock
    private CarWifiManager mCarWifiManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowCarWifiManager.setInstance(mCarWifiManager);
        mContext = ApplicationProvider.getApplicationContext();
        mFragment = new WifiSettingsFragment();
        mFragmentController = FragmentController.of(mFragment);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @After
    public void tearDown() {
        ShadowWifiManager.reset();
    }

    @Test
    public void onWifiStateChanged_disabled_setsSwitchUnchecked() {
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                PackageManager.FEATURE_WIFI, /* supported= */ true);
        when(mCarWifiManager.isWifiEnabled()).thenReturn(true);
        mFragmentController.setup();

        when(mCarWifiManager.isWifiEnabled()).thenReturn(false);
        mFragment.onWifiStateChanged(WifiManager.WIFI_STATE_DISABLED);

        assertThat(getWifiSwitch().isChecked()).isFalse();
    }

    @Test
    public void onWifiStateChanged_enabled_setsSwitchChecked() {
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                PackageManager.FEATURE_WIFI, /* supported= */ true);
        when(mCarWifiManager.isWifiEnabled()).thenReturn(false);
        mFragmentController.setup();

        when(mCarWifiManager.isWifiEnabled()).thenReturn(true);
        mFragment.onWifiStateChanged(WifiManager.WIFI_STATE_ENABLED);

        assertThat(getWifiSwitch().isChecked()).isTrue();
    }

    @Test
    public void onWifiStateChanged_enabling_setsSwitchChecked() {
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                PackageManager.FEATURE_WIFI, /* supported= */ true);
        when(mCarWifiManager.isWifiEnabled()).thenReturn(false);
        mFragmentController.setup();

        mFragment.onWifiStateChanged(WifiManager.WIFI_STATE_ENABLING);

        assertThat(getWifiSwitch().isChecked()).isTrue();
    }

    private MenuItem getWifiSwitch() {
        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        return toolbar.getMenuItems().get(0);
    }
}
