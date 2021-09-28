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

package com.android.car.settings.security;

import static com.android.car.settings.common.PreferenceController.AVAILABLE;
import static com.android.car.settings.common.PreferenceController.DISABLED_FOR_USER;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.car.settings.common.PreferenceControllerTestHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowUserManager;

/** Unit test for {@link SecurityEntryPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
public class SecurityEntryPreferenceControllerTest {

    private SecurityEntryPreferenceController mController;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mController = new PreferenceControllerTestHelper<>(RuntimeEnvironment.application,
                SecurityEntryPreferenceController.class).getController();
    }

    @Test
    public void getAvailabilityStatus_guestUser_disabledForUser() {
        getShadowUserManager().addUser(UserHandle.myUserId(), "name", UserInfo.FLAG_GUEST);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    @Test
    public void getAvailabilityStatus_nonGuestUser_available() {
        getShadowUserManager().addUser(UserHandle.myUserId(), "name", /* flags= */ 0);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadows.shadowOf(UserManager.get(mContext));
    }
}
