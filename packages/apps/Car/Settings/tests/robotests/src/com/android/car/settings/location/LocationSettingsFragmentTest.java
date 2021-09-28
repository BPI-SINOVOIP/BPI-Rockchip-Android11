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
package com.android.car.settings.location;

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import android.app.Service;
import android.content.Intent;
import android.location.LocationManager;

import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowLocationManager;
import com.android.car.settings.testutils.ShadowSecureSettings;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import java.util.List;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowSecureSettings.class, ShadowLocationManager.class})
public class LocationSettingsFragmentTest {
    private FragmentController<LocationSettingsFragment> mFragmentController;
    private LocationSettingsFragment mFragment;
    private LocationManager mLocationManager;
    private MenuItem mLocationSwitch;

    @Before
    public void setUp() {
        Robolectric.getForegroundThreadScheduler().pause();
        mLocationManager = (LocationManager) RuntimeEnvironment.application
                .getSystemService(Service.LOCATION_SERVICE);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();

        mFragment = new LocationSettingsFragment();
        mFragmentController = FragmentController.of(mFragment);
        mFragmentController.setup();
    }

    @After
    public void tearDown() {
        ShadowSecureSettings.reset();
    }

    @Test
    public void locationSwitch_toggle_shouldBroadcastLocationModeChangedIntent() {
        initFragment();
        mLocationSwitch.performClick();

        List<Intent> intentsFired = ShadowApplication.getInstance().getBroadcastIntents();
        assertThat(intentsFired).hasSize(1);
        Intent intentFired = intentsFired.get(0);
        assertThat(intentFired.getAction()).isEqualTo(LocationManager.MODE_CHANGED_ACTION);
    }

    @Test
    public void locationSwitch_checked_enablesLocation() {
        initFragment();
        mLocationSwitch.performClick();

        assertThat(mLocationManager.isLocationEnabled()).isTrue();
    }

    @Test
    public void locationSwitch_unchecked_disablesLocation() {
        initFragment();
        mLocationSwitch.performClick();

        assertThat(mLocationManager.isLocationEnabled()).isTrue();

        mLocationSwitch.performClick();

        assertThat(mLocationManager.isLocationEnabled()).isFalse();
    }

    private void initFragment() {
        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        mLocationSwitch = toolbar.getMenuItems().get(0);
    }
}
