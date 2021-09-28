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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.wifi.SoftApConfiguration;

import androidx.lifecycle.Lifecycle;
import androidx.preference.ListPreference;

import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowCarWifiManager;
import com.android.car.settings.testutils.ShadowLocalBroadcastManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowCarWifiManager.class, ShadowLocalBroadcastManager.class})
public class WifiTetherSecurityPreferenceControllerTest {

    private Context mContext;
    private ListPreference mPreference;
    private PreferenceControllerTestHelper<WifiTetherSecurityPreferenceController>
            mControllerHelper;
    private CarWifiManager mCarWifiManager;
    private WifiTetherSecurityPreferenceController mController;

    @Before
    public void setup() {
        mContext = RuntimeEnvironment.application;
        mCarWifiManager = new CarWifiManager(mContext);
        mPreference = new ListPreference(mContext);
        mControllerHelper =
                new PreferenceControllerTestHelper<WifiTetherSecurityPreferenceController>(mContext,
                        WifiTetherSecurityPreferenceController.class, mPreference);
        mController = mControllerHelper.getController();
    }

    @After
    public void tearDown() {
        ShadowCarWifiManager.reset();
        ShadowLocalBroadcastManager.reset();
        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);
        sp.edit().remove(WifiTetherPasswordPreferenceController.KEY_SAVED_PASSWORD).commit();
    }

    @Test
    public void onStart_securityTypeSetToNone_setsValueToNone() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(null, SoftApConfiguration.SECURITY_TYPE_OPEN)
                .build();
        mCarWifiManager.setSoftApConfig(config);

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(Integer.parseInt(mPreference.getValue()))
                .isEqualTo(SoftApConfiguration.SECURITY_TYPE_OPEN);
    }

    @Test
    public void onStart_securityTypeSetToWPA2PSK_setsValueToWPA2PSK() {
        String testPassword = "TEST_PASSWORD";
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(testPassword, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(Integer.parseInt(mPreference.getValue()))
                .isEqualTo(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
    }

    @Test
    public void onPreferenceChangedToNone_updatesSharedSecurityTypeToNone() {
        String testPassword = "TEST_PASSWORD";
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(testPassword, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        mController.handlePreferenceChanged(mPreference,
                Integer.toString(SoftApConfiguration.SECURITY_TYPE_OPEN));

        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);

        assertThat(sp.getInt(WifiTetherSecurityPreferenceController.KEY_SECURITY_TYPE, -1))
                .isEqualTo(SoftApConfiguration.SECURITY_TYPE_OPEN);
    }

    @Test
    public void onPreferenceChangedToWPA2PSK_updatesSharedSecurityTypeToWPA2PSK() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(null, SoftApConfiguration.SECURITY_TYPE_OPEN)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        mController.handlePreferenceChanged(mPreference,
                Integer.toString(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK));

        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);

        assertThat(sp.getInt(WifiTetherSecurityPreferenceController.KEY_SECURITY_TYPE, -1))
                .isEqualTo(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
    }
}
