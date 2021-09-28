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

package com.android.car.settings.system;

import static com.android.car.settings.common.PreferenceController.AVAILABLE;
import static com.android.car.settings.common.PreferenceController.CONDITIONALLY_UNAVAILABLE;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.Intent;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.development.DevelopmentSettingsUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowUserManager;

@RunWith(RobolectricTestRunner.class)
public class DeveloperOptionsEntryPreferenceControllerTest {

    private Context mContext;
    private PreferenceControllerTestHelper<DeveloperOptionsEntryPreferenceController>
            mControllerHelper;
    private DeveloperOptionsEntryPreferenceController mController;
    private Preference mPreference;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mPreference = new Preference(mContext);
        mPreference.setIntent(new Intent(Settings.ACTION_APPLICATION_DEVELOPMENT_SETTINGS));
        mControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                DeveloperOptionsEntryPreferenceController.class,
                mPreference);
        mController = mControllerHelper.getController();

        // Setup admin user who is able to enable developer settings.
        getShadowUserManager().addUser(UserHandle.myUserId(), "test name", UserInfo.FLAG_ADMIN);
    }

    @Test
    public void testGetAvailabilityStatus_devOptionsEnabled_isAvailable() {
        setDeveloperOptionsEnabled(true);
        assertThat(mController.getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }

    @Test
    public void testGetAvailabilityStatus_devOptionsDisabled_isUnavailable() {
        setDeveloperOptionsEnabled(false);
        assertThat(mController.getAvailabilityStatus()).isEqualTo(CONDITIONALLY_UNAVAILABLE);
    }

    @Test
    public void testGetAvailabilityStatus_devOptionsEnabled_hasUserRestriction_isUnavailable() {
        setDeveloperOptionsEnabled(true);
        getShadowUserManager().setUserRestriction(
                UserHandle.of(UserHandle.myUserId()),
                UserManager.DISALLOW_DEBUGGING_FEATURES,
                true);
        assertThat(mController.getAvailabilityStatus()).isEqualTo(CONDITIONALLY_UNAVAILABLE);
    }

    @Test
    public void performClick_startsActivity() {
        setDeveloperOptionsEnabled(true);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        mPreference.performClick();

        Intent actual = ShadowApplication.getInstance().getNextStartedActivity();
        assertThat(actual.getAction()).isEqualTo(Settings.ACTION_APPLICATION_DEVELOPMENT_SETTINGS);
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadows.shadowOf(UserManager.get(mContext));
    }

    private void setDeveloperOptionsEnabled(boolean enabled) {
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVELOPMENT_SETTINGS_ENABLED, enabled ? 1 : 0);
        DevelopmentSettingsUtil.setDevelopmentSettingsEnabled(mContext, enabled);
    }
}
