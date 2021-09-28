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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.wifi.WifiManager;

import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.testutils.ShadowCarWifiManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowCarWifiManager.class})
public class WifiRequestToggleActivityTest {

    private static final String PACKAGE_NAME = "com.android.vending";

    private Context mContext;
    private WifiRequestToggleActivity mActivity;
    private ActivityController<WifiRequestToggleActivity> mActivityController;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(PackageManager.FEATURE_WIFI,
                true);
        ShadowCarWifiManager.setInstance(new CarWifiManager(mContext));
    }

    @After
    public void tearDown() {
        ShadowCarWifiManager.reset();
    }


    @Test
    public void testOnStartWifiEnabled_toggleEnableWifi_showsDialog() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_DISABLED);

        mActivityController = createActivityController(WifiManager.ACTION_REQUEST_ENABLE);
        mActivityController.create().start();

        assertDialogShown();
    }

    @Test
    public void testOnStartWifiDisabled_toggleDisableWifi_showsDialog() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_ENABLED);

        mActivityController = createActivityController(WifiManager.ACTION_REQUEST_DISABLE);
        mActivityController.create().start();

        assertDialogShown();
    }

    @Test
    public void testOnStartWifiEnabled_toggleEnableWifi_finish() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_ENABLED);

        mActivityController = createActivityController(WifiManager.ACTION_REQUEST_ENABLE);
        mActivityController.create().start();

        assertThat(mActivityController.get().isFinishing()).isTrue();
    }

    @Test
    public void testOnStartWifiDisabled_toggleDisableWifi_finish() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_DISABLED);

        mActivityController = createActivityController(WifiManager.ACTION_REQUEST_DISABLE);
        mActivityController.create().start();

        assertThat(mActivityController.get().isFinishing()).isTrue();
    }

    private static ActivityController createActivityController(String actionRequest) {
        Intent intent = new Intent(actionRequest);
        intent.putExtra(Intent.EXTRA_PACKAGE_NAME, PACKAGE_NAME);
        return ActivityController.of(new WifiRequestToggleActivity(), intent);
    }

    private void assertDialogShown() {
        assertThat(mActivityController.get().getSupportFragmentManager().findFragmentByTag(
                ConfirmationDialogFragment.TAG)).isNotNull();
    }
}
