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
import android.text.InputType;

import androidx.lifecycle.Lifecycle;

import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.common.ValidatedEditTextPreference;
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
public class WifiTetherPasswordPreferenceControllerTest {

    private static final String TEST_PASSWORD = "TEST_PASSWORD";

    private Context mContext;
    private ValidatedEditTextPreference mPreference;
    private PreferenceControllerTestHelper<WifiTetherPasswordPreferenceController>
            mControllerHelper;
    private CarWifiManager mCarWifiManager;
    private WifiTetherPasswordPreferenceController mController;

    @Before
    public void setup() {
        mContext = RuntimeEnvironment.application;
        mCarWifiManager = new CarWifiManager(mContext);
        mPreference = new ValidatedEditTextPreference(mContext);
        mControllerHelper =
                new PreferenceControllerTestHelper<WifiTetherPasswordPreferenceController>(mContext,
                        WifiTetherPasswordPreferenceController.class, mPreference);
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
    public void onStart_securityTypeIsNotNone_visibilityIsSetToTrue() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(TEST_PASSWORD, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mPreference.isVisible()).isTrue();
    }

    @Test
    public void onStart_securityTypeIsNotNone_wifiConfigHasPassword_setsPasswordAsSummary() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(TEST_PASSWORD, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mPreference.getSummary()).isEqualTo(TEST_PASSWORD);
    }

    @Test
    public void onStart_securityTypeIsNotNone_wifiConfigHasPassword_obscuresSummary() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(TEST_PASSWORD, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mPreference.getSummaryInputType())
                .isEqualTo((InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD));
    }

    @Test
    public void onStart_securityTypeIsNone_visibilityIsSetToFalse() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(null, SoftApConfiguration.SECURITY_TYPE_OPEN)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(!mPreference.isVisible()).isTrue();
    }

    @Test
    public void onChangePassword_updatesPassword() {
        String oldPassword = "OLD_PASSWORD";
        String newPassword = "NEW_PASSWORD";

        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(oldPassword, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.handlePreferenceChanged(mPreference, newPassword);
        String passwordReturned = mCarWifiManager.getSoftApConfig().getPassphrase();

        assertThat(passwordReturned).isEqualTo(newPassword);
    }

    @Test
    public void onChangePassword_savesNewPassword() {
        String newPassword = "NEW_PASSWORD";

        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(null, SoftApConfiguration.SECURITY_TYPE_OPEN)
                .build();
        mCarWifiManager.setSoftApConfig(config);

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        mController.handlePreferenceChanged(mPreference, newPassword);

        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);

        String savedPassword = sp.getString(
                WifiTetherPasswordPreferenceController.KEY_SAVED_PASSWORD, "");

        assertThat(savedPassword).isEqualTo(newPassword);
    }

    @Test
    public void onSecurityChangedToNone_visibilityIsFalse() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(TEST_PASSWORD, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);
        sp.edit().putInt(WifiTetherSecurityPreferenceController.KEY_SECURITY_TYPE,
                SoftApConfiguration.SECURITY_TYPE_OPEN).commit();

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mPreference.isVisible()).isFalse();
    }

    @Test
    public void onSecurityChangedToWPA2PSK_visibilityIsTrue() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(null, SoftApConfiguration.SECURITY_TYPE_OPEN)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);
        sp.edit().putInt(WifiTetherSecurityPreferenceController.KEY_SECURITY_TYPE,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK).commit();

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mPreference.isVisible()).isTrue();
    }

    @Test
    public void onSecurityChangedToNone_updatesSecurityTypeToNone() {
        String testPassword = "TEST_PASSWORD";
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(testPassword, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);
        sp.edit().putInt(WifiTetherSecurityPreferenceController.KEY_SECURITY_TYPE,
                SoftApConfiguration.SECURITY_TYPE_OPEN).commit();

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mCarWifiManager.getSoftApConfig().getSecurityType())
                .isEqualTo(SoftApConfiguration.SECURITY_TYPE_OPEN);
    }

    @Test
    public void onSecurityChangedToWPA2PSK_updatesSecurityTypeToWPA2PSK() {
        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(null, SoftApConfiguration.SECURITY_TYPE_OPEN)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        String newPassword = "NEW_PASSWORD";
        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);
        sp.edit().putString(WifiTetherPasswordPreferenceController.KEY_SAVED_PASSWORD, newPassword)
                .commit();

        sp.edit().putInt(WifiTetherSecurityPreferenceController.KEY_SECURITY_TYPE,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK).commit();

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mCarWifiManager.getSoftApConfig().getSecurityType())
                .isEqualTo(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
    }

    @Test
    public void onPreferenceSwitchFromNoneToWPA2PSK_retrievesSavedPassword() {
        String savedPassword = "SAVED_PASSWORD";
        SharedPreferences sp = mContext.getSharedPreferences(
                WifiTetherPasswordPreferenceController.SHARED_PREFERENCE_PATH,
                Context.MODE_PRIVATE);
        sp.edit().putString(WifiTetherPasswordPreferenceController.KEY_SAVED_PASSWORD,
                savedPassword).commit();

        SoftApConfiguration config = new SoftApConfiguration.Builder()
                .setPassphrase(null, SoftApConfiguration.SECURITY_TYPE_OPEN)
                .build();
        mCarWifiManager.setSoftApConfig(config);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        sp.edit().putInt(WifiTetherSecurityPreferenceController.KEY_SECURITY_TYPE,
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK).commit();

        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);

        assertThat(mCarWifiManager.getSoftApConfig().getPassphrase()).isEqualTo(savedPassword);
    }
}
