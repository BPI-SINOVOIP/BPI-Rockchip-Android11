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

import static android.os.UserManager.DISALLOW_NETWORK_RESET;

import static com.android.car.settings.common.PreferenceController.AVAILABLE;
import static com.android.car.settings.common.PreferenceController.DISABLED_FOR_USER;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.preference.Preference;

import com.android.car.settings.common.PreferenceControllerTestHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowUserManager;

/** Unit test for {@link ResetNetworkEntryPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
public class ResetNetworkEntryPreferenceControllerTest {

    private ResetNetworkEntryPreferenceController mController;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;

        mController = new PreferenceControllerTestHelper<>(
                mContext,
                ResetNetworkEntryPreferenceController.class,
                new Preference(mContext)).getController();
    }

    @Test
    public void getAvailabilityStatus_nonAdminUser_disabledForUser() {
        getShadowUserManager().setIsAdminUser(false);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    @Test
    public void getAvailabilityStatus_adminUser_available() {
        getShadowUserManager().setIsAdminUser(true);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }

    @Test
    public void getAvailabilityStatus_adminUser_restricted_disabledForUser() {
        getShadowUserManager().setIsAdminUser(true);
        getShadowUserManager().setUserRestriction(
                UserHandle.of(UserHandle.myUserId()), DISALLOW_NETWORK_RESET, true);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadow.extract(UserManager.get(mContext));
    }
}
