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

import static android.os.UserManager.DISALLOW_FACTORY_RESET;

import static com.android.car.settings.common.PreferenceController.AVAILABLE;
import static com.android.car.settings.common.PreferenceController.DISABLED_FOR_USER;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;

import androidx.preference.Preference;

import com.android.car.settings.common.PreferenceControllerTestHelper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowUserManager;

/** Unit test for {@link MasterClearEntryPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
public class MasterClearEntryPreferenceControllerTest {
    private static final int SECONDARY_USER_ID = 10;

    private Context mContext;
    private MasterClearEntryPreferenceController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;

        mController = new PreferenceControllerTestHelper<>(mContext,
                MasterClearEntryPreferenceController.class,
                new Preference(mContext)).getController();
    }

    @After
    public void tearDown() {
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVICE_DEMO_MODE, 0);
    }

    @Test
    public void getAvailabilityStatus_nonAdminUser_disabledForUser() {
        createAndSwitchToSecondaryUserWithFlags(/* flags= */ 0);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    @Test
    public void getAvailabilityStatus_adminUser_available() {
        createAndSwitchToSecondaryUserWithFlags(UserInfo.FLAG_ADMIN);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }

    @Test
    public void getAvailabilityStatus_adminUser_restricted_disabledForUser() {
        createAndSwitchToSecondaryUserWithFlags(UserInfo.FLAG_ADMIN);
        getShadowUserManager().setUserRestriction(
                UserHandle.of(SECONDARY_USER_ID), DISALLOW_FACTORY_RESET, true);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    @Test
    public void getAvailabilityStatus_demoMode_demoUser_available() {
        createAndSwitchToSecondaryUserWithFlags(UserInfo.FLAG_DEMO);
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVICE_DEMO_MODE, 1);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }

    @Test
    public void getAvailabilityStatus_demoMode_demoUser_restricted_disabledForUser() {
        createAndSwitchToSecondaryUserWithFlags(UserInfo.FLAG_DEMO);
        getShadowUserManager().setUserRestriction(
                UserHandle.of(SECONDARY_USER_ID), DISALLOW_FACTORY_RESET, true);
        Settings.Global.putInt(mContext.getContentResolver(),
                Settings.Global.DEVICE_DEMO_MODE, 1);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    private void createAndSwitchToSecondaryUserWithFlags(int flags) {
        getShadowUserManager().addUser(SECONDARY_USER_ID, "test name", flags);
        getShadowUserManager().switchUser(SECONDARY_USER_ID);
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadows.shadowOf(UserManager.get(mContext));
    }
}
