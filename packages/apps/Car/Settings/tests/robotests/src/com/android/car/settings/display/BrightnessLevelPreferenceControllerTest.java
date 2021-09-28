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

package com.android.car.settings.display;

import static com.android.settingslib.display.BrightnessUtils.GAMMA_SPACE_MAX;
import static com.android.settingslib.display.BrightnessUtils.convertLinearToGamma;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.os.UserHandle;
import android.provider.Settings;

import androidx.lifecycle.Lifecycle;

import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.common.SeekBarPreference;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(RobolectricTestRunner.class)
public class BrightnessLevelPreferenceControllerTest {
    private Context mContext;
    private BrightnessLevelPreferenceController mController;
    private SeekBarPreference mSeekBarPreference;
    private int mMin;
    private int mMax;
    private int mMid;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = RuntimeEnvironment.application;
        mMin = mContext.getResources().getInteger(
                com.android.internal.R.integer.config_screenBrightnessSettingMinimum);
        mMax = mContext.getResources().getInteger(
                com.android.internal.R.integer.config_screenBrightnessSettingMaximum);
        mMid = (mMax + mMin) / 2;

        mSeekBarPreference = new SeekBarPreference(mContext);
        PreferenceControllerTestHelper<BrightnessLevelPreferenceController>
                preferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                BrightnessLevelPreferenceController.class, mSeekBarPreference);
        mController = preferenceControllerHelper.getController();
        preferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
    }

    @Test
    public void testRefreshUi_maxSet() {
        mController.refreshUi();
        assertThat(mSeekBarPreference.getMax()).isEqualTo(GAMMA_SPACE_MAX);
    }

    @Test
    public void testRefreshUi_minValue() {
        Settings.System.putIntForUser(mContext.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS, mMin, UserHandle.myUserId());

        mController.refreshUi();
        assertThat(mSeekBarPreference.getValue()).isEqualTo(0);
    }

    @Test
    public void testRefreshUi_maxValue() {
        Settings.System.putIntForUser(mContext.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS, mMax, UserHandle.myUserId());

        mController.refreshUi();
        assertThat(mSeekBarPreference.getValue()).isEqualTo(GAMMA_SPACE_MAX);
    }

    @Test
    public void testRefreshUi_midValue() {
        Settings.System.putIntForUser(mContext.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS, mMid, UserHandle.myUserId());

        mController.refreshUi();
        assertThat(mSeekBarPreference.getValue()).isEqualTo(
                convertLinearToGamma(mMid,
                        mMin, mMax));
    }

    @Test
    public void testHandlePreferenceChanged_minValue() throws Settings.SettingNotFoundException {
        mSeekBarPreference.callChangeListener(0);
        int currentSettingsVal = Settings.System.getIntForUser(mContext.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS, UserHandle.myUserId());
        assertThat(currentSettingsVal).isEqualTo(mMin);
    }

    @Test
    public void testHandlePreferenceChanged_maxValue() throws Settings.SettingNotFoundException {
        mSeekBarPreference.callChangeListener(GAMMA_SPACE_MAX);
        int currentSettingsVal = Settings.System.getIntForUser(mContext.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS, UserHandle.myUserId());
        assertThat(currentSettingsVal).isEqualTo(mMax);
    }

    @Test
    public void testHandlePreferenceChanged_midValue() throws Settings.SettingNotFoundException {
        mSeekBarPreference.callChangeListener(convertLinearToGamma(mMid, mMin, mMax));
        int currentSettingsVal = Settings.System.getIntForUser(mContext.getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS, UserHandle.myUserId());
        assertThat(currentSettingsVal).isEqualTo(mMid);
    }
}
