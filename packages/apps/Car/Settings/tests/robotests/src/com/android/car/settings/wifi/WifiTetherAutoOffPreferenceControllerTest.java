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

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiManager;

import androidx.lifecycle.Lifecycle;
import androidx.preference.SwitchPreference;
import androidx.preference.TwoStatePreference;

import com.android.car.settings.common.PreferenceControllerTestHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(RobolectricTestRunner.class)
public class WifiTetherAutoOffPreferenceControllerTest {

    private Context mContext;
    private TwoStatePreference mTwoStatePreference;
    private PreferenceControllerTestHelper<WifiTetherAutoOffPreferenceController> mControllerHelper;
    @Mock
    private WifiManager mWifiManager;
    private SoftApConfiguration mSoftApConfiguration;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = spy(RuntimeEnvironment.application);

        when(mContext.getSystemService(WifiManager.class)).thenReturn(mWifiManager);
        mSoftApConfiguration = new SoftApConfiguration.Builder().build();
        when(mWifiManager.getSoftApConfiguration()).thenReturn(mSoftApConfiguration);

        mTwoStatePreference = new SwitchPreference(mContext);
        mControllerHelper =
                new PreferenceControllerTestHelper<WifiTetherAutoOffPreferenceController>(mContext,
                        WifiTetherAutoOffPreferenceController.class, mTwoStatePreference);
    }

    @Test
    public void onStart_tetherAutoOff_on_shouldReturnSwitchStateOn() {
        setAutoOffSetting(true);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(mTwoStatePreference.isChecked()).isTrue();
    }

    @Test
    public void onStart_tetherAutoOff_off_shouldReturnSwitchStateOff() {
        setAutoOffSetting(false);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(mTwoStatePreference.isChecked()).isFalse();
    }

    @Test
    public void onSwitchOn_shouldReturnAutoOff_on() {
        setAutoOffSetting(false);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        mTwoStatePreference.performClick();

        assertThat(getAutoOffSetting()).isEqualTo(true);
    }

    @Test
    public void onSwitchOff_shouldReturnAutoOff_off() {
        setAutoOffSetting(true);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        mTwoStatePreference.performClick();

        assertThat(getAutoOffSetting()).isEqualTo(false);
    }

    private boolean getAutoOffSetting() {
        ArgumentCaptor<SoftApConfiguration> softApConfigCaptor =
                ArgumentCaptor.forClass(SoftApConfiguration.class);
        verify(mWifiManager).setSoftApConfiguration(softApConfigCaptor.capture());
        return softApConfigCaptor.getValue().isAutoShutdownEnabled();
    }

    private void setAutoOffSetting(boolean config) {
        mSoftApConfiguration =
                new SoftApConfiguration.Builder(mSoftApConfiguration)
                        .setAutoShutdownEnabled(config)
                        .build();
        when(mWifiManager.getSoftApConfiguration()).thenReturn(mSoftApConfiguration);
    }
}
