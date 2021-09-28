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

package com.android.car.settings.wifi;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.wifi.WifiManager;

import androidx.lifecycle.Lifecycle.Event;
import androidx.preference.Preference;

import com.android.car.settings.R;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowCarWifiManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import java.util.Arrays;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowCarWifiManager.class})
public class WifiStatusPreferenceControllerTest {
    private static final List<Integer> VISIBLE_STATES = Arrays.asList(
            WifiManager.WIFI_STATE_DISABLED,
            WifiManager.WIFI_STATE_ENABLING);
    private static final List<Integer> INVISIBLE_STATES = Arrays.asList(
            WifiManager.WIFI_STATE_ENABLED,
            WifiManager.WIFI_STATE_DISABLING,
            WifiManager.WIFI_STATE_UNKNOWN);

    private Context mContext;
    private Preference mPreference;
    private WifiStatusPreferenceController mController;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                PackageManager.FEATURE_WIFI, true);
        mPreference = new Preference(mContext);
        PreferenceControllerTestHelper<WifiStatusPreferenceController> controllerHelper =
                new PreferenceControllerTestHelper<>(mContext,
                        WifiStatusPreferenceController.class, mPreference);
        mController = controllerHelper.getController();
        ShadowCarWifiManager.setInstance(new CarWifiManager(mContext));
        controllerHelper.sendLifecycleEvent(Event.ON_CREATE);
    }

    @After
    public void tearDown() {
        ShadowCarWifiManager.reset();
    }

    @Test
    public void updateState_wifiNotAvailable_invisible() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_UNKNOWN);

        mController.refreshUi();

        assertThat(mPreference.isVisible()).isFalse();
    }

    @Test
    public void updateState_wifiDisabled_visible() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_DISABLED);

        mController.refreshUi();

        assertThat(mPreference.isVisible()).isTrue();
    }

    @Test
    public void updateState_wifiDisabled_isNotSelectable() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_DISABLED);

        mController.refreshUi();

        assertThat(mPreference.isSelectable()).isFalse();
    }

    @Test
    public void updateState_wifiDisabled_showsDisabledText() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_DISABLED);

        mController.refreshUi();

        assertThat(mPreference.getTitle())
            .isEqualTo(mContext.getResources().getString(R.string.wifi_disabled));
    }

    @Test
    public void updateState_wifiDisabled_showsIcon() {
        ShadowCarWifiManager.setWifiState(WifiManager.WIFI_STATE_DISABLED);

        mController.refreshUi();

        assertThat(mPreference.getIcon()).isNotNull();
    }

    @Test
    public void onWifiStateChanged_invisible() {
        for (int state : INVISIBLE_STATES) {
            ShadowCarWifiManager.setWifiState(state);
            mController.onWifiStateChanged(state);
            assertThat(mPreference.isVisible()).isEqualTo(false);
        }
    }

    @Test
    public void onWifiStateChanged_visible() {
        for (int state : VISIBLE_STATES) {
            ShadowCarWifiManager.setWifiState(state);
            mController.onWifiStateChanged(state);
            assertThat(mPreference.isVisible()).isEqualTo(true);
        }
    }

    @Test
    public void onWifiStateChanged_disabled() {
        int state = WifiManager.WIFI_STATE_DISABLED;
        ShadowCarWifiManager.setWifiState(state);
        mController.onWifiStateChanged(state);

        assertThat(mPreference.isSelectable()).isFalse();
        assertThat(mPreference.getIcon()).isNotNull();
        assertThat(mPreference.getTitle())
                .isEqualTo(mContext.getResources().getString(R.string.wifi_disabled));
    }

    @Test
    public void onWifiStateChanged_enabling() {
        int state = WifiManager.WIFI_STATE_ENABLING;
        ShadowCarWifiManager.setWifiState(state);
        mController.onWifiStateChanged(state);

        assertThat(mPreference.isSelectable()).isTrue();
        assertThat(mPreference.getIcon()).isNull();
        assertThat(mPreference.getTitle())
                .isEqualTo(mContext.getResources().getString(R.string.loading_wifi_list));
    }
}
